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

Both shaders are **static**: the whole register state of the corresponding block is passed as uniforms, so nothing has to be recompiled when the game reconfigures the GFX registers. The TEV shader walks up to 16 combine stages in a loop that is bounded by a `tevStages` uniform.

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

### Textures

The Flipper has eight texture maps. Every map is decoded from main memory into an RGBA image and kept in its
own GL texture object (`TextureEngine`), so a TEV configuration that uses several maps can bind them all at
once. Texture coordinates are scaled by the ratio between the real texture size and the power-of-two size of
the GL image (`texScale` uniform).

![gfx_class](/wiki/imgstore/emu/gfx_class.png)

Each pipeline class (`xf`, `su`, `ras`, `tx`, `tev`, `pe`, `bump`) holds a reference to the parent `gfx` instance
and owns its slice of the register state; `ras` turns the accumulated vertices of a draw command into a GL draw
call and binds the program, the uniforms and the textures.

## Not emulated yet

- Bump mapping and indirect texturing;
- the Z-texture environment (`TEV_Z_ENV_0/1` is stored but not applied);
- direct access to the EFB (Cpu2Efb);
- the Z-compare path of the pixel engine.

## Debugging

- `EMU_LOG=<file>` — dump all `Debug::Report` messages to a text file (useful when the debugger window is not open);
- `GFX_DUMP=<prefix>`, `GFX_DUMP_EVERY=<n>` — write every n-th rendered frame to `<prefix>_NNNNNN.bmp`.
