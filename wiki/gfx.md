# Flipper GFX

The entire emulation of the Flipper graphics Pipeline begins in the gfx.cpp module. Two completely different approaches are planned:
- Simulation of the entire GFX subsystem using vertex/fragment shaders
- Software emulation, with faithful copying of EFB -> XFB and other GFX blocks

The GFX subsystem is quite complex, even by 2026 standards, so it's not surprising that there's still a lot to be done.

## Shader pipeline

The graphics backend no longer uses the fixed-function OpenGL pipeline. The two programmable stages of the Flipper GFX are mapped onto the two programmable stages of OpenGL (GL 3.3 / GLSL 330):

| Flipper | OpenGL | Where |
|---|---|---|
| XF (Transform Unit) | vertex shader | one static shader, source in `xf.cpp` |
| TEV (Texture Environment Unit) | fragment shader | one static shader, source in `tev.cpp` |

Both shaders are **static**: the whole register state of the corresponding block is passed as uniforms, so nothing has to be recompiled when the game reconfigures the GFX registers. The TEV shader walks up to 16 combine stages in a loop that is bounded by a `tevStages` uniform. The one program variant that exists is the flat-shaded one: `GEN_MODE.flat_en` requires the `flat` qualifier on the rasterized colour varyings on *both* sides of the link, so a second vertex stage and a second fragment source are built when the bit is set (`TransformUnit::VertexShaderSource(bool)`, `TextureEnvironmentUnit::FragmentShaderSource(bool)`, and the `GetTevProgram` cache that relinks when the bit flips).

### Command path

The CP produces the command stream and owns the vertex fetch; the XF is the entry point of the pipeline, so
every word the CP emits goes through it:

```
Gekko --PI FIFO--> CP --commands + vertex rows--> XF --> SU --> RAS --> TEV --> PE
```

The CP pushes the XF register block loads (`xf_cmd_regload` + `regdata`), register read requests
(`xf_cmd_regread`), SU bypass register words and the vertex rows of every draw command into the XF
(`xf.h`, the "CP -> XF interface" section), and observes the `XFready` line before each word. A register
read is answered by the XF out of its own register state and latched in `CP_XF_DATAL` / `CP_XF_DATAH`,
where the CPU sees it. The CP does not reach past the XF: the vertex stream leaves the XF towards the SU,
which drives the rasterizers.

### XF (vertex shader)

- geometry (modelview) and texture matrix multiplies against the matrix RAM (`matrixMem`, 64 rows x 4 words) and
  the dual-texture matrix RAM;
- the projection combine (`ProjectionA`..`ProjectionF` + `ProjectOrtho`);
- per-vertex lighting for the two colour channels: material/ambient sources, N.L diffuse evaluation
  (constant / signed / clamped) and the cosine (`a0`+`a1`cos+`a2`cos^2) and distance (`1/(k0+k1d+k2d^2)`)
  attenuation fractions;
- texture coordinate generation: regular (source row + 2x4/3x4 matrix), colour texgen and the Rev-B dual transform;
- the geometry matrix comes from the per-vertex matrix index word (CP), the normal matrix from the same
  index masked to 5 bits.

### TEV (fragment shader)

Per stage the shader decodes the raw `TEV_COLOR_ENV`/`TEV_ALPHA_ENV` register payloads (`uvec4` uniforms) and evaluates

```
result = ( D +/- lerp(A, B, C) + bias ) << shift , then clamped
```

All arithmetic is carried in units of 1/255, which matches the hardware (8-bit colour values, 11-bit signed
colour registers). Each stage result is written back into the colour register file (four registers, kept in
local variables) and the last stage result is scaled down to [0,1] for the framebuffer. The stage's texture
comes from `RAS1_TREF` (texture map / texture coordinate / texture enable) and the rasterized colour from
the TREF colour-source field. Fog and the final alpha function are applied after the stage chain.

The fog factor is computed from the window depth, the range adjustment, A and C (gfx-tev.md 3.6) and then
goes through the F-select law of `TEV_FOG_PARAM_3.fsel`. The five laws the GX API programs are 2 (linear),
4 (exponential), 5 (exponential squared), 6 (backward exponential) and 7 (backward exponential squared);
the two encodings it cannot produce are decoded as the family in `fsel[2:1]` plus the "square the value"
select in `fsel[0]`, which makes 1 the "off" family with the square bit set (no fog, like 0) and 3 the
linear law applied to the squared value (a quadratic curve).

### Global state the backend applies

Besides the per-block state above, the backend honours the setup unit's output control registers:

| Register | Effect |
|---|---|
| `SU_SCIS0` / `SU_SCIS1` | the scissor rectangle in screen coordinates (top left origin) with the +342 bias of the SU, converted into a GL scissor box (`glScissor`), clamped to the render target and to a non-negative size; it resets to the whole screen |
| `SU_LPSIZE.lsize` / `.psize` | the GL line width and point size (the register is in 1/16 pixel increments, floored at one pixel and clamped to what the context reports) |
| `SU_SSIZE` / `SU_TSIZE` | the coordinate scale of a texture coordinate pair in texels (`ssize + 1`), composed with the sampler's padding correction |
| `X` viewport registers | `glViewport` (see the XF section) |
| `GEN_MODE.flat_en` | the flat-shaded program variant (see above) |

### Textures

The Flipper has eight texture maps. Every map is decoded from main memory into an RGBA image and kept in its
own GL texture object (`TextureEngine`), so a TEV configuration that uses several maps can bind them all at
once. Texture coordinates are scaled by the ratio between the real texture size and the power-of-two size of
the GL image (`texScale` uniform), and by the coordinate scale of the setup unit (`suScale` uniform, the
`SU_SSIZE`/`SU_TSIZE` size of the pair minus one, see below); the two are composed, so the padding correction
is not replaced by the register.

The mip selection of a map comes from the sampler registers: `TX_SETMODE0.lodbias` (an s2.5 bias in 1/32 of a
level) is `GL_TEXTURE_LOD_BIAS`, `TX_SETMODE1.minlod`/`maxlod` (each unsigned 4.4, in 1/16 of a level) are
`GL_TEXTURE_MIN_LOD`/`GL_TEXTURE_MAX_LOD`, which is the order the texture unit applies them in. A reset
`TX_SETMODE1` (0, 0) therefore pins the sampler to level 0, exactly like the hardware.

![gfx_class](/wiki/imgstore/emu/gfx_class.png)

Each pipeline class (`xf`, `su`, `ras`, `tx`, `tev`, `pe`, `bump`) holds a reference to the parent `gfx` instance
and owns its slice of the register state; `ras` turns the accumulated vertices of a draw command into a GL draw
call and binds the program, the uniforms and the textures.

## Not emulated yet

- **Bump mapping and indirect texturing.** The indirect texturing *is* implemented (see below); the
  bump/indirect register file (BP 0x06-0x1F: the three 3x2 indirect matrices, `BUMP_IMASK` and one
  indirect command per TEV stage) is emulated and can be dumped with the `gx`/`gxregs bump` debug
  commands.
- **XF texgen type 1 (bump texgen).** Not implemented. gfx-xf.md says only that the light unit emits
  (s, t) offsets for up to three bump stages and does not give the offset scale; the XF inside
  Flipper is a microcode ROM, so no block-level description of that part of the chip exists (there is
  no bump/emboss logic to read). The patent US6825851 ("Bump Mapping") describes
  the technique rather than the register-level formula: texture coordinates come from the eye-space X
  and Y of the converted normal, scaled by the offsets (dF/dx, dF/dy) read from the bump map, with
  signed offsets, the 3x2 matrix of the bump unit and a scale that "should contain the size of the
  reflection map divided by 2 (and thus the reflection map should be a square power of 2)". That is
  enough to implement the technique, but not enough to guarantee the exact coordinate the hardware
  produces, so the coordinate is still passed through.

## Indirect texturing

Implemented in the TEV fragment shader (`tev.cpp`), following gfx-bump.md 3.3-3.8: for every TEV
stage whose indirect command is not `bp_m_off`, the stage's coordinate (S17.7, i.e. texel units with
seven fraction bits) is masked by its `sw`/`tw` wrap window, the indirect texture named by `bt` is
sampled there, the texel fields are decoded per `fmt` (8/5/4/3 bits), biased per component, multiplied
by the selected 3x2 matrix (including the special "coordinate window" modes), shifted into the 25-bit
coordinate window as `(dot << scale)[44:20]` and added back, together with the previous stage's offset
when `bp_fb` is set. The perturbed coordinate is what the stage samples its own texture with.

The coordinate the indirect stage starts from is scaled by the shift scale of that indirect stage
(`RAS1_SS0`/`RAS1_SS1`, `ras1_sts` 0..8 = divide by 1..256, gfx-ras1.md 4.2): the `bt` field of a stage's
indirect command is the bump stage the TEV stage consumes, so `bt` selects which of the four shift pairs
applies.

The patent US6707458 ("Indirect Textures") confirms the shape of that chain - indirect sample ->
offset -> matrix and scale -> modulo wrap of the coordinate -> add -> final lookup, with signed
offsets - but it does not contain the arithmetic itself: the description refers the pipeline details
to the incorporated application Ser. No. 09/722,382, which is not part of the available corpus.

- **The copy engine's texture and display copies.** `PE_COPY_CMD` is decoded and its clear
  operation is executed (with the `PE_XBOUND`/`PE_YBOUND` bounds and the PE clear values); the
  EFB→main-memory (texture copy) and EFB→XFB (display copy) paths need the EFB to be addressable as
  a texture, which the GL backend does not provide.
- **Direct access to the EFB (Cpu2Efb).** `EfbPeek`/`EfbPoke` are still stubs. A correct
  implementation needs a CPU-side EFB image: the CPU reaches the EFB (0x08000000, bit 22 = the Z
  plane) from the Gekko thread, where no OpenGL context is current, so the read/write has to be
  deferred to the render thread.
- **The Z compression format (`PE_CONTROL.zcmode`) and `ztop`.** The mode only changes the precision
  of the depth buffer; the backend always uses a 24-bit depth buffer, which is the uncompressed
  mode. `ztop` (early vs late Z) has no GL equivalent in this backend.
- **PE dither (`PE_CMODE0.dither_en`).** The register is applied to `GL_DITHER`, but that cannot
  reproduce the hardware: the Flipper dithers with a 4x4 ordered matrix when the EFB pixel format
  stores fewer bits per channel than the TEV produces (RGB565 or RGBA6), while this backend renders
  into an 8-bit-per-channel target for every format. For the RGB8 EFB format the matrix is the
  identity, and for the narrower ones there is no render target to quantise into. A faithful
  implementation has to start with the EFB pixel format, which the emulator does not decode.
- **`BUMP_IMASK` (`0x0F`).** Not a mask of the indirect texel components: the bump unit's entry stage
  uses its eight payload bits to classify incoming *words*, forwarding a texture-group register word
  on the short path when the mask bit selected by the word's tag is set (gfx-bump.md 3.2, 4.3); the GX
  API fills it with the texture maps its indirect stages fetch from. The backend decodes every BP word
  straight into the register file, so there is no stream to route and the mask cannot change a
  picture. It is kept in the register file and in the debugger dump.
- **The per-pair flag fields of `SU_SSIZE`/`SU_TSIZE`.** `bs`/`ws`/`bt`/`wt` are the
  bias and cylindrical-wrap enables, which only preserve precision of a repeated fixed-point
  coordinate (the backend works in floats, where the integer bias has no visible effect), and
  `lf`/`pf` enable the line/point texture offsets `SU_LPSIZE.ltoff`/`ptoff` (a sub-texel shift of the
  texture coordinate on wide lines and points, which the GL line/point rasterizer does not
  reproduce).
- **`PE_CMODE1.yuv`** (the YUV write path) is decoded but not applied.
- **Anti-aliasing and the field mask.** `GEN_MODE.ms_en`, `GEN_MSLOC0..3` and `PE_FIELD_MASK` are
  decoded and visible in the register dumps; the render target is a single-sample buffer, so
  3-sample AA and interlaced field rendering are not reproduced.

## Texture formats

The component expansion follows the Flipper format definitions (gfx-tc.md 5.1-5.4), which the unit
tests now pin down:

| Format | Expansion |
|---|---|
| I4 | the 4-bit value repeated (`{v,v}`) |
| I8 | the byte copied into R, G, B and A |
| IA4 | intensity = the **low** nibble of the byte, alpha = the **high** nibble, each repeated |
| IA8 | alpha = the **high** byte of the big-endian texel, intensity = the **low** byte |
| RGB565 | 5-bit red and blue and 6-bit green repeated to 8 bits (`{[4:0],[4:2]}`), alpha 255 |
| RGB5A3 | top bit set: RGB555 with alpha 255; clear: R/G/B 4-bit repeated and a **3-bit** alpha repeated (`{a,a,a[2:1]}`) |
| RGBA8 | alpha = the high byte and red the low byte of the first tile, green and blue in the second |
| C4/C8/C14X2 | a palette index into the TLUT; the palette entry uses the IA8/RGB565/RGB5A3 expansion above |

The palette lookup (`TX_LOADTLUT0/1` → `TX_SETTLUT`) and the C4/C8 index path are exercised by the
tests as well.

The tile shapes come from the texture unit (gfx-tc.md 5.3): 4-bit formats are
8x8 texels per 32-byte unit, the 8-bit ones (I8/IA4/C8) are 8 wide by 4 tall, the 16-bit ones
(IA8/RGB565/RGB5A3/C14X2) are 4x4, and RGBA8 is a 4x4 half in two consecutive 32-byte units (the
AR unit first, then the GB one). CMPR sub-blocks sit in the 8x8 tile in the order TL, TR, BL, BR at
byte offsets 0/8/16/24.

The tiles of an image are stored **row by row**: every tile of the first tile row comes first, then
the tiles of the second one, and so on. An image that is wider than one tile therefore needs the
tile row as the outer walk of the decoder; with the tile columns outer the map is read column by
column and everything but the first tile column is transposed. This is what happened to the IA4
decoder (8-wide tiles) and it is why a 32x32 IA4 map came out as noise while the same map was
correct in the gallery - the gallery encoder mirrored the decoder's walk. Both walks are row-first
now, and `gfx_texture_test.cpp` samples the first texel of every tile of a 2x2 tile grid for the
8x8 (I4), 8x4 (IA4) and 16-bit/RGBA8 formats so the order is pinned.

The C14X2 palette index is 14 bits wide (the register field passes all of them through), and the
transparent fourth colour of the CMPR three-colour mode keeps the average of its two endpoints with
a cleared alpha, which is what the hardware puts there (the RGB takes part in a blend that uses it,
only the alpha makes the texel invisible).

### Decoding on demand

A draw programs the whole texture map (`GXLoadTexObj`) every time it uses it, so the backend is
asked for a decode constantly. `DecodeTexture` only converts and uploads when the map really
changed: it compares the description (base address, format, size, palette binding) and an FNV-1a
hash of the raw texture bytes it would read against what the GL texture was decoded from
(`TexMap::key*`). A palette lives outside the texture bytes, so every TLUT load bumps a generation
counter that is part of the check as well (`gfx-tc.md` palettes are shared by the maps that bind
them). Titles that edit a texture in place are still picked up, because the hash covers the bytes,
not just the registers; `Tex_AnInPlaceEditOfTheTexelsIsPickedUp` and
`Tex_AReloadedPaletteIsPickedUp` pin that down.

`TX_INVTAGS` (`GXInvalidateTexAll`) marks every map for a decode again. The upload reuses the GL
image storage (`glTexSubImage2D`) while the image size is unchanged, and the sampler parameters
(including the mip chain of a mipmapped map) are only re-applied when `TX_SETMODE0/1` really
changes, not on every write of the same value.

**Open item (CMPR).** The format definition available to the project describes the two interpolated
endpoints as thirds (`(2*A + B) / 3`), which is what the decoder implements, while a second
description of the same decoder gives `A*(frac+1) + B*(8-frac-1)` shifted right by three, i.e. 3/8
and 5/8 with truncation, and a three-colour mode of `(col0 + col1) / 2` with alpha 0. Changing the
weights touches every CMPR texture in every title, so it needs a test that decodes a block with
known endpoints and indices first.



`testing/` has a unit test suite for the whole subsystem (see `testing/Readme.md`): it drives the
real OpenGL backend, compares the TEV combine against an independent reference model written from
gfx-tev.md 3.2, runs the XF vertex shader through transform feedback, walks the BP register space
to make sure no register is silently dropped, and executes display lists through the CP FIFO. The
rendering tests write their screenshots into an HTML report
(`x64/<Config>/gfx_test_out/gfx_report.html`).

## Debugging

- `EMU_LOG=<file>` — dump all `Debug::Report` messages to a text file (useful when the debugger window is not open);
- `GFX_DUMP=<prefix>`, `GFX_DUMP_EVERY=<n>` — write every n-th rendered frame to `<prefix>_NNNNNN.bmp`.

### JDI commands

The GFX subsystem registers its own JDI node (`GFX_JDI_JSON`, declared in `gfx.cpp`), so the GX
state is reachable from the debugger and from the JDI server (issue #87):

| Command | What it does |
|---|---|
| `gx` | The state of the whole pipeline: the common `GEN_MODE`, and the XF/SU/RAS/TEV/PE/bump state plus the frame counters |
| `gxframes` | The frame counters and the CP's per-frame counters (BP/XF/CP loads, triangles, points, lines) |
| `gxregs <block>` | The register dump of one block: `xf`, `su`, `ras`, `tx`, `tev`, `pe`, `bump`, `cp` or `all` |
| `gxshader <base>` | Write the GLSL sources the pipeline uses to `<base>.vert.glsl` and `<base>.frag.glsl` |
| `gxtex` | The texture cache: what every one of the eight texture maps holds and where it comes from |
| `gxtexdump <map> <file.png>` | Decode one texture map and save it as a PNG |
| `gxshot <file.png> [x y w h]` | Save the emulated EFB as a PNG (the whole render target by default) |
| `gxpixel <x> <y>` | Read one EFB pixel: colour and depth |
| `gxreset` | Reset the GFX register state (the software equivalent of a GX reset) |

The commands that read the EFB (`gxshot`, `gxpixel`, `gxtexdump`) need a current OpenGL context, so
they report an error instead of crashing when they are called from a thread that does not drive the
frame loop.

