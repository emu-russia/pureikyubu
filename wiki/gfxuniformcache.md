# GFX uniform cache

The shader pipeline ([gfx.md](gfx.md)) does not use the fixed-function pipeline: the two programmable
stages of the Flipper are two **static** GL programs, and everything the hardware registers say
travels to them as uniforms. A draw therefore re-uploads the whole register state of the blocks -
the XF matrices, the eight light records, the TEV environments of all 16 stages, the fog and alpha
constants, the texture coordinate scales - and the call that does it is
`Rasterizer::SetUpPipeline`:

```
RAS_End -> SetUpPipeline -> program->Use()
                         -> xf->UploadUniforms(program)      (xf.cpp)
                         -> tev->UploadUniforms(program)     (tev.cpp -> tx->UploadTexScales)
```

That is around sixty `glUniform*` calls per draw, and a title that draws the same material over and
over (the usual case: a scene is a run of primitives with the same TEV setup and the same matrices)
repeats exactly the same sixty calls, with exactly the same values, for every primitive of the run.
`src/gfxuniformcache.cpp` / `src/gfxuniformcache.h` is the module that drops those calls: the value
every uniform was last uploaded with is kept, and an upload whose value has not changed never
reaches the GL context.

The instance lives in the GFX engine itself (`GFXCore::uniformCache`), and the blocks upload through
it instead of calling `glUniform*`:

```cpp
// The XF hands the light records to the program (xf.cpp)
gfx->uniformCache.Set4fv(p, "lightRgba[0]", 8, (float*)rgba);
```

The cache can be switched off, which is what the setting below and the `gxuniformcache` command are
for: the uploads of a draw then reach the GL context exactly as they did before the cache existed,
which is what a picture that a stale uniform is suspected of is compared against.

| Upload of the cache | GL entry point it stands for |
|---|---|
| `Set1f` | `glUniform1f` |
| `Set1i` | `glUniform1i` |
| `Set4f` | `glUniform4f` |
| `Set1fv`, `Set2fv`, `Set4fv` | `glUniform1fv` / `2fv` / `4fv` |
| `Set1iv` | `glUniform1iv` |
| `Set1uiv`, `Set2uiv`, `Set4uiv` | `glUniform1uiv` / `2uiv` / `4uiv` |

Every one of them is the same three steps:

1. the value the cache holds for the name is compared with the value of the call;
2. a value that differs - or that the cache does not hold at all - resolves the location through
   `GLProgram::Uniform` (which caches the locations) and is uploaded, then remembered;
3. a value that is equal is dropped: the program keeps what it already holds.

The comparison is **bitwise** (`memcmp` of the bytes the `glUniform*` call would carry), which is the
granularity the GL call itself works at: it copies the raw value into the uniform storage of the
program, so bytes that are equal leave that storage exactly as it is. A float that differs in its
last mantissa bit is an upload; an array of which one item moved is an upload of the whole array
(the call carries the whole array anyway).

## What a value belongs to

A uniform value is not identified by its name alone. `UniformCache` keys an entry on

* the **name** of the uniform (how the call sites address it; the location of a linked program never
  changes), and
* the **identity of the uniform storage** it was uploaded to - `GLProgram::serial`, handed out by
  `NextProgramSerial()`.

The identity is what makes the cache safe, because neither the `GLProgram` object nor its GL name can
tell one program from another:

* GL **reuses the names** of the programs that were deleted, so a program that was linked in the
  place of another one can carry the name the cache already has values for;
* a program that was **linked again has no uniform set at all** - the values of a fresh program are
  the defaults - while the register state of the draw may be exactly the state the cache holds, so a
  name-only cache would skip every upload and the draw would run with a program of zeros.

`serial` is renewed whenever the storage is replaced: by the `GLProgram` constructor, by
`GLProgram::Link` (the object links a program of its own) and by `GLProgram::Destroy` (the program is
gone and the object may be given another one later). One relink of the pipeline is what makes this
matter in practice: `GEN_MODE.flat_en` changes the *program* (`TextureEnvironmentUnit::GetTevProgram`
deletes the TEV program and links the flat-shaded variant), and the register state of the draw around
it is untouched.

## Settings and the debugger command

| Where | What |
|---|---|
| `hardware.GFX_UNIFORM_CACHE` | `1` (the shipped default) = the cache drops an upload whose value has not changed, `0` = every upload of a draw reaches the GL context. Read at start-up |
| Settings window, **Core Settings → The rendering backend → Uniform cache** | The checkbox next to the graphics backend. It writes the configuration variable and switches the machine that is running at once |
| `gxuniformcache [on\|off]` | The debugger / JDI command: reports the state of the cache or switches it. The settings window runs this same command, so the two do exactly the same thing |

Switching the cache drops the values it holds, in both directions: while it is off every upload goes
through, so what it remembers describes a context that has moved on. The first draw after the switch
therefore uploads its whole state, and the skipping resumes from the draw after it. The setting is
the host's copy of the GL context and not part of the emulated state, so like the pipeline it is not
carried by a save state; the choice itself travels in the configuration, so the next start makes it
again.

## When the cache is dropped

The cache is the host's copy of what the GL context holds, so it is dropped whenever that copy
cannot describe the context any more:

| Where | Why |
|---|---|
| `GFXCore::SetPipeline` | the pipeline switch: the software pipeline never uploads anything, and switching back closes and reopens the GL backend, so the values of the pipeline that was left must not be taken for the values of the new context |
| `GFXCore::GL_CloseSubsystem` | the context goes away and the programs it held (the TEV program above all) are destroyed with it |
| `GLProgram::Link` / `GLProgram::Destroy` | the uniform storage of that object is replaced (see above) |

The register state itself does **not** invalidate anything, and does not have to: a change of a
register changes the upload the state produces, and the comparison notices it. A reset of the
pipeline (`GFXCore::ResetPipelineState`, the `gxreset` command) is the same case - the values the
reset produces are either the ones the context already holds (the upload is dropped, correctly) or
new ones (the upload happens).

## What is not cached

* The **debugger's own GL** (`debugui2gl.cpp`: the ImGui renderer, the font atlas texture) and the
  **HW profiler overlay** (`gfxosd.cpp`) own programs of their own and upload to them directly with
  `glUniform*`. They are not part of the emulated pipeline and do not share its program.
* The **software pipeline**: it has no GL context and no uniforms at all. A switch to it drops the
  cache (see the table above), so a state that comes back to the shader pipeline always uploads its
  first draw in full.

Everything the pipeline itself uploads goes through the cache, the **sampler bindings**
(`texMap0`..`texMap7`) included: those are given to a program once, when the TEV program is linked
(`TextureEnvironmentUnit::GetTevProgram`), and they are remembered like any other value.

## Tests

`testing/gfx_uniform_cache_test.cpp` drives the cache through the real GL backend and reads the
uniform back out of the context (`glGetUniformfv`), so it never has to count the calls of the cache:
an upload the cache dropped is visible as the value another writer left in the uniform.

| Test | What it pins down |
|---|---|
| `UniformCache_AnUnchangedValueIsNotUploaded` | the first upload of a value reaches the program, the second one is dropped (a value written behind the cache's back survives), a changed value is uploaded, a scalar and an array, and `Invalidate` lets the next upload through |
| `UniformCache_ARegisterChangeReachesTheProgramAndThePicture` | a draw uploads the state through the cache: a TEV colour register that moved reaches both the program and the EFB |
| `UniformCache_TheSwitchTurnsTheSkippingOffAndOn` | the switch (the setting and the command, `GFXCore::SetUniformCache`): off, an upload that carries what the uniform holds reaches the context; the choice lands in the configuration; on again, the first upload of every value goes through |
| `UniformCache_ARelinkedProgramIsUploadedAgain` | the flat-shaded variant of the TEV program is a program of its own: the value the cache holds for the name is uploaded to it instead of being skipped |
| `UniformCache_ThePipelineSwitchDoesNotLeaveStaleValuesBehind` | the same state is drawn, the pipeline is switched to the software one and back, and the same state is drawn again: the picture is the same one, which can only happen if the whole state was uploaded to the program that was linked after the switch |

## Open items

* The cache keeps **one entry per uniform name**, and the entry names the storage it belongs to. A
  second program that the same names are uploaded to (the transform feedback program the GFX tests
  link, `GfxTestMachine::RunVertexShader`) therefore replaces the entry of the pipeline and its own
  values are uploaded again on the next draw - correct, but not cached across the two. The pipeline
  itself has one program, so this costs nothing in the emulator.
* Nothing is cached for the per-vertex stream: that is the vertex buffers, not uniforms, and the
  debugger's ImGui atlas and the profiler overlay are out of scope (see above).
