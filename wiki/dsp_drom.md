# DSP DROM (`build/Data/dsp_drom.bin`)

![dsp_drom_map](/wiki/imgstore/emu/dsp_drom_map.png)

This page is an **analysis of the data itself** — the 4 KiB DSP data ROM that `DspCore` maps
at data address `0x1000` (`DspCore::DROM_START_ADDRESS`, `DspCore::DROM_SIZE = 4 * 1024`) and
that `Flipper` loads from `Config::DspDromFilename` (`src/flipper.cpp`).

Everything below was measured from the image with the emulator's own definitions; no other
emulator was consulted and no assumption is made about how the image was authored.

## 1. Shape of the image

| Item | Value |
|---|---|
| File size | 4096 bytes |
| Organisation | 2048 big-endian 16-bit words (`struct.unpack('>2048H')`) |
| DSP data space | `0x1000` … `0x1FFF` (`TranslateDMem` maps `(addr-0x1000) << 1` bytes) |
| Word 3 | the only word whose value equals its own index (`0x0003`) |

Reading the file as 16-bit **big-endian** words is what makes the payload look ordered; the same
bytes read little-endian look like noise. One exception: the very end of the image is
byte-oriented (`0xDEAD`, `0xBEEF`, an ASCII marker), so the tail must be read as bytes rather
than as table entries.

## 2. `0x0000` – `0x0BFF`: three palindromic "quad" cells

This region is one construction repeated three times, 1024 bytes (512 words) each. Inside a cell
the data is **four lanes interleaved 8 bytes apart** — lane `k` is `word[4n + k]` — and the whole
cell is an **exact word-level palindrome**:

```
word[i] == word[511 - i]        for every i in the cell
```

Palindromes were located by scanning for the longest `word[c-1-k] == word[c+k]` runs:

| Axis | Run | Covered span |
|---|---|---|
| byte `0x0200` | `k = 256` words | `0x0000` – `0x0400` |
| byte `0x0600` | `k = 256` words | `0x0400` – `0x0800` |
| byte `0x0A00` | `k = 256` words | `0x0800` – `0x0C00` |

Because each cell is a palindrome, only its first 512 bytes are independent data; the second
half is the first half read backwards.

| Cell | Byte range | First word | Peak `abs` | Independent half |
|---|---|---|---|---|
| 0 | `0x0000` – `0x03FF` | `6600` | `19426` | `0x000` – `0x1FF` |
| 1 | `0x0400` – `0x07FF` | `3195` | `26287` | `0x400` – `0x5FF` |
| 2 | `0x0800` – `0x0BFF` | `-68` | `32639` | `0x800` – `0x9FF` |

`32639 = 2^15 - 129`, i.e. the third cell comes very close to full 16-bit amplitude.

### 2.1 Lane structure

Reading lane `k` as `word[4n+k]`, `n = 0 … 127` (the independent half of cell 0):

| Lane | Word offset | Sweep | Step size | Shape |
|---|---|---|---|---|
| A | `word[4n+0]` | `6600 → 3` | `-121 → -6` | monotone, convex |
| B | `word[4n+1]` | `19426 → 6722` | `-2 → -123` | monotone, concave |
| C | `word[4n+2]` | `6722 → 19426` | `+123 → +2` | monotone, concave |
| D | `word[4n+3]` | `3 → 6600` | `+6 → +121` | monotone, convex |

The four lanes are not four independent waveforms, but two mirror pairs:

* **lane D is the exact reverse of lane A**: `D[n] == A[127 - n]` (both meet at `3`);
* **lane C is the exact reverse of lane B**: `C[n] == B[127 - n]` (both meet at `6722`).

Consequently the even and odd combinations are exactly even/odd about the cell centre:
`A + D` and `B + C` are symmetric, `A − D` and `B − C` are antisymmetric. Together with the cell
palindrome this means the independent payload of the whole 3 KiB is just **three 512-byte
halves**, i.e. two lanes per cell.

### 2.2 The lanes are smooth quantised curves

None of the lanes is a sine wave: every one of them is monotone, and its first difference changes
by only `-3 … +3` from sample to sample. Taking lane A as the example, the second difference is
non-negative everywhere and cycles through `0, 1, 2, 3` — the curve is convex and slowly
flattening (`-121 … -6` per step). The mutual mirror relations above then generate the rest.

The byte planes show the same thing: in the first 3 KiB the high bytes ramp monotonically
(76 distinct values in the first lane) while the low bytes cycle through ~170 distinct values,
which is the signature of an ordered fixed-point sweep rather than of audio PCM.

### 2.3 Amplitude growth

The three cells are the same construction with increasing amplitude; the measured peak grows
`19426 → 26287 → 32639`. Comparing lane by lane, the later cells are scaled copies of the
cell-0 lanes (for cell 1, lane B/`A ≈ 1.35` and lane C/`A ≈ 1.68`), and the higher the amplitude,
the more high-order structure the curve acquires — which is what one expects from one fixed
relative amplitude resolution sampled at three different gains.

## 3. `0x0C00` – `0x0FFF`: coefficient fields and test data

The boundary at `0x0C00` is clearly visible in the figure, both as a jump in the value statistics
and as a switch from smooth to "speckled" byte planes. The palindrome structure collapses from
`k = 256` words to a handful of short runs.

| Byte range | Content | Measured detail |
|---|---|---|
| `0x0C00` – `0x0CFF` | dense coefficient field | 9 zero words; short palindromes at `0x0C14` (`k=10`), `0x0C64` (`k=20`), `0x0CA0` (`k=10`) |
| `0x0D00` – `0x0DFF` | sparse coefficient lanes | 21 zero words; values sit on odd word lanes only (`0`, `-419`, `0`, `864`, …) |
| `0x0E00` – `0xF5F` | test vector | long mirror; only 2 zero words |
| `0xF60` – `0xFFF` | zero fill + unit impulses + marker | 40 zero words |

The tail is the most "digital" part of the image and is not a waveform at all:

* `0xF8C` – `0xFEC`: a train of exactly **28 words with the value `1`** (unity in 16.16), with
  the spacing shrinking `4,4,3,3,2,2,2,2,2,2,2,1,2,1,2,1,1,2,1,1,2,1,1,1,1,1,1` — a ramp that
  ends in a solid run;
* `0xFE0` – `0xFEB`: the same impulses on alternating words;
* `0xFEC`: `00 01 45 55 47 20 54 45 4E 46 4E 41 48 43 52 4C` — the ASCII fragments
  `EUG`, `TENF`, `NAHC`, `RL`;
* `0xFFC` – `0xFFF`: `DE AD BE EF`.

The ASCII fragments are byte-oriented: read as big-endian words the text becomes
`E U G` `T E N F` `N A H C` `R L`, which is neither random nor meaningful English, and no 1/2-bit
rotation of the bytes turns it into a word either. Treat the block as a signature/marker, not as
a string the emulator has to parse.

## 4. What the statistics say

* **Redundancy.** The 4096-byte image carries 2560 bytes of independent information
  (3 cells × 512 independent bytes + the 1024-byte literal tail); the rest is the mirror
  symmetry — the image is 60 % larger than its information content.
* **Compressibility.** The image is essentially *incompressible*: `xz -9` yields 3720 bytes,
  `zlib -9` 3765, `bzip2` 4047. The low bits therefore behave like real quantisation noise at
  roughly 1 bit per sample, not like arithmetic that could be factored out.
* **Smoothness.** A least-squares description of one lane as
  `a + b·n + c·n² + d·n³ + e·n⁴ + P·cos(ωn) + Q·sin(ωn)` reaches rms ≈ 0.29 LSB with a maximum
  error ≈ 0.57 LSB — i.e. right at the 16-bit quantisation floor, confirming the payload is a
  *smooth quantised waveform* rather than random data, while the residual noise is what makes the
  bytes incompressible.
* **No unique closed form.** Asking the stronger quantisation question — is there a function whose
  every value lands inside its sample's ±0.5 LSB window? — is a linear program
  (`min t s.t. |M(n)·c − y[n]| ≤ t`). Measured on the image, `t* < 0.5` (feasible) is reached by a
  degree-3 polynomial plus one sinusoid for cell 0, and by degree 5 for cells 1 and 2, whereas a
  pure polynomial needs degree ≥ 7. But feasibility also holds for most carrier frequencies and
  for several degrees, and forcing all three cells to share one frequency pushes `t*` back to
  ≈ 0.9, so the ±0.5 LSB window is too wide to identify a unique underlying expression.
* **Practical consequence for the emulator.** The image is a data ROM: the emulator reads the
  bytes as they are (`TranslateDMem(0x1000)`, `LoadDrom`), and the structure above is what makes
  the format redundant enough to describe compactly — mirror symmetry halves the first 3 KiB, and
  two lanes per cell are mirrors of the other two.

## 5. Related material

* `testing/DspIrom.md` — the instruction ROM (`dsp_irom.bin`, 8 KiB at program `0x8000`), its
  reset/mailbox/DSP-DMA code and its core self-test.
* `wiki/flipper.md` — where the DSP sits in the Flipper block diagram.
* `src/dspcore.h` / `src/dspcore.cpp` — `DROM_START_ADDRESS`, `TranslateDMem`, `LoadDrom`.
* `testing/dsp_bench/bench.cpp` — hashes the DROM (`TranslateDMem(0x1000)`) as a benchmark.
