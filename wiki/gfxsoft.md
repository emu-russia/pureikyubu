# Flipper GFX - the software pipeline (experimental)

> **The software pipeline is experimental.** It renders the whole pipeline by itself and the
> DolphinSDK demo sweep agrees with the shader backend on most of the demos, but a few titles still
> have picture defects (the anti-aliased framebuffer demos, a handful of texture and copy cases);
> they are being worked through one by one. The default stays the shader backend.

The second rendering path of the GFX subsystem, selected by the `GFX_PIPELINE` configuration
variable (`0` = the OpenGL shader backend of [gfx.md](gfx.md), `1` = software) and switchable at run
time with the `gxpipeline` JDI command. The choice is written back to the settings file, so it
survives a restart.

It is a path of its own: it never opens an OpenGL context and shares no rendering state with the
shader backend. The two implementations live side by side in the same modules (`xf.cpp`, `su.cpp`,
`ras.cpp`, `tx.cpp`, `tev.cpp`, `pe.cpp`) and are marked as such; the software methods of a block
carry the `Soft` prefix.

There is no GL frame in this pipeline at all. The blocks render into a **real EFB memory array**
and the copy engine converts the finished frame into the **XFB in main memory**, which `vi.cpp`
scans out exactly like a real console (the number of active lines comes from `VI_VERT_TIMING`, so a
title that copies the 448 visible lines of an NTSC frame does not show the rest of the buffer):

```
Gekko --PI FIFO--> CP --commands--> XF(soft) --> SU(soft) --> RAS(soft) --> TEV(soft) --> PE(soft)
                                                                  ^            ^
                                                          TX(soft, TMEM)      |
                                                                              v
                                                              EFB memory --(copy engine)--> XFB
                                                                                              |
                                                                                      vi.cpp scans it out
```

The unit tests of this pipeline (`testing/gfx_soft_test.cpp`) drive it through the hardware API
only - the XF register space, the BP register space and the object-space vertex stream - and read
the result out of the EFB memory and the XFB, without an OpenGL call.

## XF

A high-level transformation of the vertex, not an interpreter of the XF microcode: the geometry
and texture matrix multiplies, the projection combine, the per-vertex lighting (the two channels,
material/ambient sources, N.L diffuse, the cosine and distance attenuation), the texture
coordinate generation (regular, colour, dual transform) and the bottom of the pipe - the divide by
the homogeneous component and the viewport mapping (`X'' = (Xc/Wc)*Sx + Ox`, gfx-xf.md 3.2). The
result is a window-space vertex: X/Y in EFB pixels (the origin is the top left corner, Y grows
downward), Z in 24-bit depth units and `1/w` for the perspective correction of the rasterizers.

A viewport that was never programmed maps the whole render target, which is the default the shader
backend gets from GL.

The bottom of the pipe also **clips** (gfx-xf.md 3.5): every triangle is cut against the near and
far planes and the four guard-band planes of the hardware clipper (x/y = ±2w, so a primitive
between w and 2w survives and the scissor of the setup unit cuts it). The clipper runs in clip
space, where the attributes are linear, and the vertices it inserts interpolate them with the same
parameter as the clip position; without it a triangle that crosses the camera plane is projected
through the origin and lands in the wrong half of the screen. Two details the specification leaves
open are resolved the way the rest of the emulator needs them: the near plane is cut a ten-
thousandth of the triangle's own depth extent in front of `w = 0`, because a vertex exactly on it
projects to infinity and the hardware answers the same singularity with the saturated fixed-point
screen position of the setup unit (gfx-su.md 5.1); and the z planes follow the `[-w, w]` band this
emulator's projection combine produces (the plane list of the specification names the near plane
"z > 0" and the far one "z < -w", which is the guard-region convention of a projection that puts
the near plane at the origin).

## SU

The primitive assembler and the setup stage: points, lines and line strips are expanded into
rasterisable geometry (gfx-su.md 3.3), the strips, fans and the Flipper quads are split into
triangles with the alternating winding of a strip, and every triangle gets the setup record the
rasterizer walks with - the bounding box, the three edge coefficients (normalized so that the
interior is positive) and the interpolation plane of every attribute (gfx-su.md 3.4 `su_area` /
`su_param`).

The perspective-correct parameters (the two colour channels and the eight texture coordinates) are
carried as planes of `value/w` next to the plane of `1/w`, which is the division RAS1 performs at
the pixel centres (gfx-ras1.md 3.3). The depth plane is screen-linear, like the Z plane of
RAS0/RAS2. `GEN_MODE.zfreeze` holds the depth plane of the first triangle of the frame, and
`GEN_MODE.reject_en` rejects the front or the back faces exactly as the shader backend's culling
does: the software window has Y growing downward while the GL window has it growing upward, so the
signed area of a triangle decides the two cases (SetupUnitSoftReject in `su.cpp` carries the
derivation, and the two pipelines are compared against each other by the demo sweep).

## RAS

The rasterizers walk the primitive the way the hardware does: on the 2x2-pixel **quad** grid, one
quad at a time. Every quad gets a 12-bit coverage mask (three sub-samples per pixel, one bit each)
and only the pixels whose mask is not empty are shaded; the pixel is evaluated either at its centre
(a fully covered pixel) or at one of its covered sub-samples (gfx-ras2.md 3.3). Samples exactly on
an edge follow the top-left rule, so two triangles sharing an edge do not both cover it.

The sub-sample positions come from `GEN_MSLOC0..3` when `GEN_MODE.ms_en` is set. The encoding of
the offsets is not fully pinned down by the available specification (it describes 1/12-pixel
distances from the quad centre while the RTL applies them with the sign of the pixel's position in
the quad); the model reads a field as a signed 1/12-pixel offset from the *pixel centre* whose
value 6 - the value the SDK's `GXInit` programs - means "no offset", so the three sub-samples of a
pixel degenerate to its centre unless a title programs a real pattern.

## TX (TMEM)

The software texture unit owns a real **TMEM**: the 32 banks of 16K x 16-bit words of gfx-tc.md
3.1, in the two 512 KB halves the 15-bit `tmem_offset` fields address. A 32-byte cache line is
written through the sixteen banks of a half; the explicit load commands (`TX_LOADBLOCK0..3`,
`TX_LOADTLUT0/1`) stream main-memory tiles and palette entries into it, and a hardware-managed
("cached") image is fetched through a tag cache in TMEM (gfx-tc.md 3.5, 3.6).

The sampler follows the level of detail of gfx-tc.md 3.3 (the texel-to-pixel ratio of the
coordinate derivatives, the `lodbias` bias and the `minlod`/`maxlod` band), the clamp / repeat /
mirror coordinate operations of 3.4 and the filter datapath of gfx-tf.md 3: the format expansion
of 5.2 (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, the three colour-index formats through the TLUT
and CMPR), the bilinear S/T lerps with 6-bit fractions, the trilinear blend with a 5-bit fraction
and the `min_filter`/`mag_filter` selection.

The software model keeps its tag cache in one region of TMEM (128 KB at the top of the low half)
rather than in the region each image programs: the cache is a performance structure and the test
and demo images program the same offset, so a per-image region would evict the other images'
lines. Everything else - the tile order, the bank/word packing and the two halves - follows the
main-memory layout of the format.

## TEV and PE

The TEV is the stage datapath of gfx-tev.md 3, run once per shaded sample: up to 16 combine stages
over the colour register file (`result = ( D +/- lerp(A, B, C) + bias ) << shift`, then clamped),
the Rev-B K constants, the alpha compare modes, the Z-texture environment (which may replace the
depth), the fog unit and the final alpha function.

**Indirect (bump) texturing** is implemented as well (gfx-bump.md 3.3-3.8): a stage whose indirect
command is not `bp_m_off` fetches its *indirect* map through the texture unit, masks the coordinate
to its wrap window, decodes the texel fields with the format and the bias of the command, multiplies
them by the selected 3x2 matrix (including the two coordinate-window modes), shifts the result into
the 25-bit S17.7 coordinate window and adds it to the stage's own coordinate, together with the
feedback of the previous indirect stage. The arithmetic is the one the shader backend uses, so the
two pipelines produce the same picture; the unit tests pin it with an indirect fetch that shifts a
stage by one texel (`Soft_AnIndirectStageSamplesItsTextureThroughTheBumpOffset`).

The pixel engine owns a real **EFB memory array**, addressed like the CPU window of the hardware
(gfx-pe.md 3.3): the colour word of the pixel (x, y) sits at `y * 1024 + x` and the address bit 22
selects the Z plane. It performs the RMW datapath of gfx-pe.md 4 - the Z test of the Z unit and the
blend / logic op with the write masks of the colour unit - and the copy engine: the display copy
that converts a rectangle into the packed YUV 4:2:2 **XFB** in main memory (5.6), the texture copy
that re-packs it into the tiled texture formats (5.7) and the clear a copy may ask for (5.1).

There is no GL frame in this pipeline: the picture the copy engine wrote into the XFB is what the
video interface scans out (`vi.cpp`), exactly like a real console.

## What the software pipeline does not do yet

- The per-vertex "emboss" bump texgen of the XF (texgen type 1): the incoming coordinate is passed
  through.
- The anisotropic filtering (`maxaniso`), the `diag_lod` and `lodclamp` refinements of the LOD, and
  the `round` / `field_predict` motion-compensation modes of the filter.
- The anti-aliased EFB (a 12-bit coverage mask selects the sample to shade, but the EFB stores one
  colour per pixel) and the EFB pixel types other than RGB8 - which is also why the dither matrix
  is not applied: for RGB8 it is the identity.
- The `PE_COPY_VFILTER` coefficients (the display copy scales vertically with the `PE_COPY_SCALE`
  lerp, but the 7-tap filter is a no-op), the YUV/4:2:0 copy modes and the EFB CPU window
  (`Cpu2Efb`).
- The Rev-B `PE_CHICKEN` behaviours (`tx_copy_fmt`, `txcpy_ccv`, the Rev-A/Rev-B blend-op and CPU
  Z-mask fixes) and the `BUMP_IMASK` stream mask, which routes register words on the hardware and
  has no stream to route here.

The unit tests of the software pipeline (`testing/gfx_soft_test.cpp`) drive it through the
hardware API only - the XF register space, the BP register space and the object-space vertex
stream - and read the result out of the EFB memory and the XFB, without an OpenGL call.


## Pictures

![The scene the software pipeline rendered](/wiki/imgstore/emu/gfx_soft_scene.png)

The scene above is drawn entirely by the software pipeline, in the order the unit test submits it:
a full-screen quad with a colour gradient (the four-corner interpolation of RAS2), a triangle whose
vertex colours interpolate across it, a red quad that is nearer than the triangle (so the Z unit of
the pixel engine rejects the triangle's samples behind it) and a quad whose texels are read out of
TMEM. The picture is the EFB memory, read back through the pixel engine's own API.

![The frame after the display copy](/wiki/imgstore/emu/gfx_soft_xfb.png)

The same frame after `GXCopyDisp`: the copy engine converted the EFB rectangle into packed
YUV 4:2:2 in main memory, and this picture is that XFB decoded back to RGB. The chroma of every
pixel pair is averaged by the 4:2:2 downsampling, which is why the edges of the two rectangles show
a chroma transition.

Both pictures are produced by the test suite (`Soft_ReportThePipelineRendersAScene` and
`Soft_ReportTheXfbTheCopyEngineWrote`) and published into the test report.
