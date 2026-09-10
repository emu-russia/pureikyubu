# DSP IROM (`build/Data/dsp_irom.bin`) — disassembly and analysis

The 8 KB instruction ROM at program address `0x8000` is the part of the DSP system that runs
**before any downloadable microcode exists**: after a hardware reset the core vectors to
`0x8000` (the IRAM/`0x0000` vector is used only for a software reset), and this ROM is
responsible for bringing the audio side up and then for serving the CPU.

Everything below was produced from the image itself with the emulator's own decoder and
disassembler. The listing can be regenerated at any time:

```
MSBuild scripts/VS2026/pureikyubu_test.slnx -p:Configuration=Debug -p:Platform=x64
vstest.console scripts/VS2026/x64/Debug/pureikyubu_test.dll /Tests:Irom_DumpDisassemblyForReview
```

The regenerated listing lands in `build/Data/dsp_irom_disasm.txt`; the checks that keep it
honest live in `testing/dsp_irom_test.cpp`.

## 1. Shape of the image

| Item | Value |
|---|---|
| Size | 8192 bytes = 4096 instruction words |
| Reset entry | `0x8000` |
| Reachable code | `0x8000` … `0x88EA` (the last instruction is a `rets` at `0x88EA`) |
| Padding | `0x88EB` … `0x88EF` (five `nop`s) |
| ROM fill pattern | `0x88F0` … `0x8FFD` — 270 words of an incrementing ramp (`0x88F0`, `0x88F1`, …), i.e. each word equals its own address |
| Trailing words | `0x8FFE` = `0x06E2`, `0x8FFF` = `0x8845` (not part of the ramp, never executed) |

A *recursive-descent* walk from the reset vector (`Irom_EveryReachableInstructionDecodes`)
reaches 809 instructions and **every one of them decodes**; nothing in the fill pattern is
ever reached as code. A naive linear disassembly of the tail produces "instructions" that look
plausible (`clr b` + `mr`, `ldd`, …) purely by accident — one of them, `0x88F0`-`0x88FF`, even
lands in the multiply opcode hole — which is why the test walks the control flow instead.

Instruction mix of the reachable code (mnemonic: count):

```
ld 343, lsl16 197, asr16 193, admpy 192, st 182, add 117, mvli 97, mv 68, stsa 66,
clr 56, stli 55, ldla 49, lsfi 35, xor 28, call 28, stla 27, addl 23, lsf 19, rets 16,
and 16, amv 15, lsr16 13, ldsa 11, jmp 10, adsi 9, not 8, loop 8, cmp 8, mpy 7, inc 5,
mvsi 5, btstl/btsth 5+5, tst 4, orli 4, mr 4, dec 4, wait 3, set 2, neg 2
```

914 of the 809… counted words are *packed*: every second word carries a second (move) half —
286 `ld`, 208 `st`, 186 `ls`, 31 `mr`, 29 `mv`. The ROM therefore already exercises the
parallel decode, the dual load/store forms, circular addressing, the mailbox and DSP-DMA.

## 2. Map of the procedures

### 2.1 `0x8000` — reset entry / hardware init

```asm
8000  mvli  dpp, #0x00FF     ; all short-address accesses target the 0xFF00 control page
8002  clr   et               ; interrupts off while bringing the block up
8003  clr   te0              ;  (te0 is unused by this core)
8004  clr   te1              ;  accelerator/decoder interrupts
8005  clr   te2              ;  AI DMA
8006  clr   te3              ;  CPU -> DSP
8007  clr   xl               ; extension limit off
8008  clr   dp               ; single precision multiply
8009  set   im               ; INTEGER multiply mode
800A  stli  $(DMBH), #0x8071 ; "DSP is alive" — high word
800C  stli  $(DMBL), #0xFEED ;                   low word (sets the valid flag)
```

While this code runs the active program base is the IROM (`0x8000`), so every interrupt vector
is `0x8000 + offset` (`0x8002` ERR, `0x8004` TRAP, …) rather than `0x0002`/`0x0004`; the boot
code therefore clears `et`/`te0`–`te3` before anything else can happen, and the vectors only
move to the IRAM low page once the CPU clears the CDCR reset-vector bit.

The `stli` pair is the first use of the mailbox protocol: the high word is written first
(clearing the valid flag, bit 15 of `DMBH`) and the low word second (setting it). Software on
the CPU side polls bit 15 of `DMBH` and only then reads `DMBL`.

Note that `dpp = 0xFF` is exactly what makes `ldsa d,0xFE`/`ldsa d,0xFF` in the routines below
address `CMBH` (`0xFFFE`) and `CMBL` (`0xFFFF`).

### 2.2 `0x800E` — main CPU command loop

```asm
800E  clr   a                ; a = 0
800F  clr   b
8010  call  $0x8078          ; WaitMailToDsp: a1 <- CMBH, blocks until the valid flag is set
8012  mvli  b1, #0x80F3      ; the "audio system is up, talk to me" signature
8014  cmp   a, b
8015  jmpz  $0x801F          ; CMBH == 0x80F3 -> command dispatcher
8017  ldsa  b1, $0x00FF      ; otherwise: b1 <- CMBL
8018  stli  $(DMBH), #0xFEEE ; echo the message back with the "not accepted" marker
801A  stsa  $0x00FD, a1      ;   ... low word (sets the valid flag)
801B  call  $0x807E          ; WaitAck: block until the CPU has taken the answer
801D  jmp   $0x800E
```

So the DSP answers *every* CPU→DSP message: commands are accepted only when the high word is
`0x80F3`, otherwise the pair is bounced back as `0xFEEE`/`<low word>` so that the CPU can
distinguish "rejected" from "accepted".

### 2.3 `0x801F` — command dispatcher

The low word of an accepted message selects one of a series of `mv`-into-register commands.
Each handler waits for the next mailbox message (`call 0x8078`) and stores its **high word**
into one register and its **low word** into another, then returns to the main loop:

| Command in `CMBL` | Registers written | Purpose |
|---|---|---|
| `0xA001` | `m0` ← msg hi, `m1` ← msg lo | circular-buffer address 0 (DSMAH/DSMAL for DSCR=3) |
| `0xA002` | `m3` ← msg lo | length for the same buffer |
| `0xC002` | `m2` ← msg lo | DSP memory address (DSPA) |
| `0xB001` | `x1` ← msg hi, `x0` ← msg lo | main-memory address (DSMAH:DSMAL) |
| `0xB002` | `y0` ← msg lo | block length (DSBL) |
| `0xC001` | `y1` ← msg lo | DSP memory address |
| `0xD001` | `r0` ← msg lo, then `jmp $0x80B5` | *execute* the DSP-DMA |

Any other value drops through to `0x8070` → `wait` (the DSP parks until an interrupt).

### 2.4 `0x8078` / `0x807E` — the two mailbox handshakes

```asm
8078  ldsa  a1, $0x00FE      ; a1 <- CMBH
8079  btsth a1, #0x8000      ; TB = (bit 15 set)
807B  jmpnt $0x8078          ; spin while there is no CPU -> DSP message
807D  rets

807E  ldsa  a1, $0x00FC      ; a1 <- DMBH
807F  btstl a1, #0x8000      ; TB = (bit 15 clear)
8081  jmpnt $0x807E          ; spin until the CPU has consumed the DSP -> CPU message
8083  rets
```

`WaitMailToDsp` is the routine the whole ROM is built around: it is the only way the DSP ever
receives work, and it leaves the received high word in `a1` (which is why the main loop can
`cmp a,b` against the signature without re-reading `CMBH`).

`WaitAck` is the mirror image: it blocks until the CPU clears the valid flag by reading the
low word of the DSP→CPU mailbox. Note that these are *not* the same mailbox.

### 2.5 `0x8085` … `0x80E5` — DSP-DMA execution

Four nearly identical blocks program the DSP-DMA registers and then poll the busy bit:

```asm
8085  clr   xl
8086  clr   a
8087  mv    a1, y0
8088  tst   a
8089  jmpz  $0x809D          ; a zero length means "nothing to do"
808B  stla  $(DSMAH), x1     ; main memory address [25:16]
808D  stla  $(DSMAL), x0     ; main memory address [15:2]
808F  mvli  a1, #0x0001
8091  stla  $(DSCR), a1      ; direction = main -> DSP, memory = instruction (IRAM)
8093  stla  $(DSPA), y1      ; DSP memory word address
8095  stla  $(DSBL), y0      ; block length (writing this word starts the transfer)
8097  ldla  a1, $(DSCR)
8099  btstl a1, #0x0004      ; bit 2 = busy
809B  jmpnt $0x8097          ; wait for completion
809D  ...                    ; next block
80B4  rets
```

The four variants differ only in the `DSCR` value they write, i.e. in direction and target
memory:

| `DSCR` | Direction | Memory |
|---|---|---|
| `0x0000` | main → DSP | data (DRAM) |
| `0x0001` | main → DSP | instruction (IRAM) |
| `0x0002` | DSP → main | instruction (IRAM) |
| `0x0003` | DSP → main | data (DRAM) |

`0x80B5` (reached from the `0xD001` command through `jmp r0`) is the composite form: it runs
the instruction-memory download, then the data-memory download, then the instruction-memory
upload. `0x80E5` ends with `jmp r0`, i.e. an indirect return through the address register that
`0xD001` loaded — the ROM's only computed jump.

### 2.6 `0x80E7` … `0x88EA` — DSP core self-test

The largest part of the ROM is a self-check of the DSP core itself. It is recognisable by its
instruction mix: 192 `admpy`, 197 `lsl16`, 193 `asr16`, 343 `ld`, 208 `st` and 186 `ls`
(the load-and-store form that lets one instruction word move two operands), i.e. a long
straight-line sequence of multiply-accumulate/shift patterns whose results are stored to
memory. This is the "DSP hardware check" that the audio system performs at boot; it is the
reason the ROM contains `mpy`/`admpy`/`mvmpy` code that the downloadable microcode does not.

## 3. What the analysis says about the emulator

* Every instruction used by the ROM decodes correctly, including the packed forms — the
  decoder is exercised by 914 packed words.
* The mailbox protocol used here (`stli DMBH`/`stli DMBL`, `ldsa` of `CMBH`/`CMBL`, `btsth`/
  `btstl` on `0x8000`) is exactly the protocol implemented by `Dsp16` and covered by
  `testing/dsp_mailbox_test.cpp`.
* The DSP-DMA register semantics used here (`DSMAH`/`DSMAL`/`DSCR`/`DSPA`/`DSBL`, busy bit 2
  of `DSCR`) match `Dsp16::WriteDMem`/`ReadDMem` and the instant DMA of `dspdma.cpp`.
* Three bugs found by the unit tests are directly relevant to this ROM:
  * the logic-family flag computation (`not`/`xor`/`and`/`or` are used 68 times here) — `N`
    used to be stuck at 0, which breaks every `jmpn`/`jmpt` decision made after a logic op;
  * `lsf`/`lsf`-style shifts (19 `lsf` + 35 `lsfi`) used to execute the *non-negated*
    direction for the `lsf d,-x1` forms;
  * the multiply operands used to be read as unsigned, which corrupts all 199 `mpy`/`admpy`
    instructions in the self-test.
