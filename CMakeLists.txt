cmake_minimum_required (VERSION 3.10)
project (pureikyubu)

# The headless build (issue #382): the emulator without a window, a game selector or an OpenGL
# context. The front end is uinull.cpp, the renderer is the null GFX backend (gfxnull.cpp, selected
# by GFX_NULL) and the video/audio/pad/console back ends are the null ones. It is a console
# application, meant for unattended runs and for `--bench` (see src/uinull.cpp).
option (HEADLESS "Build the headless emulator (no window, no SDL/GL front end)" OFF)

# Choose an X86/X64 build

if (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(X86 OFF)
  set(X64 ON)
else ()
  set(X86 ON)
  set(X64 OFF)
endif ()

find_package (Threads REQUIRED)
find_package(OpenGL REQUIRED)

if (APPLE)
  # Build the bundled dependencies for the selected host architecture.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/SDL2 EXCLUDE_FROM_ALL)

  add_library(pureikyubu_glew STATIC thirdparty/glew-2.2.0/src/glew.c)
  target_include_directories(pureikyubu_glew PUBLIC thirdparty/glew-2.2.0/include)
  target_compile_definitions(pureikyubu_glew PUBLIC GLEW_STATIC)
  target_link_libraries(pureikyubu_glew PUBLIC OpenGL::GL)
else ()
  find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
  if (NOT HEADLESS)
    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
  endif ()
  find_package(GLEW REQUIRED)
endif ()

add_definitions (-D_LINUX)
#add_definitions (-DCMAKE_BUILD_TYPE=Debug)

if (HEADLESS)
  add_definitions (-DGFX_NULL)
endif ()

# ../GekkoCore/Gekko.h:196:66: warning: `fastcall` attribute ignored [-Wattributes]
add_compile_options(-Wno-attributes)

# The sources are UTF-8 (issue #372). GCC and Clang read them as UTF-8 by themselves; MSVC has to
# be told, or it falls back to the ANSI code page of whoever builds the project and misreads the
# non-ASCII text the sources contain (the Japanese application name in ui.h, the arithmetic signs
# in the comments). The narrow literals become UTF-8 bytes as well, which is the encoding the
# emulator's interfaces speak.
if (MSVC)
  add_compile_options(/utf-8)
endif ()

# The emulator core: everything that is not tied to a particular front end or output device.

set (PUREIKYUBU_CORE
	src/ai.cpp
	src/bootrtc.cpp
	src/bump.cpp
	src/config.cpp
	src/cont.cpp
	src/cp.cpp
	src/debug.cpp
	src/di.cpp
	src/dsp.cpp
	src/dspcore.cpp
	src/dspjit_x64.cpp
	src/dspjit_x86.cpp
	src/dspdebug.cpp
	src/dspdec.cpp
	src/dspdisasm.cpp
	src/dspai.cpp
	src/dsparam.cpp
	src/dspdma.cpp
	src/dvd.cpp
	src/dvddebug.cpp
	src/exi.cpp
	src/flipper.cpp
	src/flipperdebug.cpp
	src/gekko.cpp
	src/gekkoc.cpp
	src/gekkojit_x64.cpp
	src/gekkojit_ps_x64.cpp
	src/gekkojit_x86.cpp
	src/gekkojit_ps_x86.cpp
	src/gekkodebug.cpp
	src/gekkodec.cpp
	src/gekkodisasm.cpp
	src/gfx.cpp
	src/gfxuniformcache.cpp
	src/gfxdump.cpp
	src/gfxosd.cpp
	src/hwdebug.cpp
	src/hwosd.cpp
	src/hsp.cpp
	src/bba.cpp
	src/mdm.cpp
	src/gbplayer.cpp
	src/gbalink.cpp
	src/hwprof.cpp
	src/jdi.cpp
	src/jdiserver.cpp
	src/jdispecs.cpp
	src/json.cpp
	src/main.cpp
	src/mcp.cpp
	src/mem.cpp
	src/memcard.cpp
	src/os.cpp
	src/osdebug.cpp
	src/pe.cpp
	src/peripherals.cpp
	src/pi.cpp
	src/ras.cpp
	src/rvz.cpp
	src/savestate.cpp
	src/si.cpp
	src/su.cpp
	src/sym.cpp
	src/tev.cpp
	src/tx.cpp
	src/uijdi.cpp
	src/utils.cpp
	src/vi.cpp
	src/xf.cpp

	thirdparty/zstd/zstddeclib.c
)

# The front end and the platform back ends.

if (HEADLESS)
  set (PUREIKYUBU_FRONTEND
	src/audionull.cpp
	src/bbaudp.cpp
	src/bench.cpp
	src/gfxnull.cpp
	src/padnull.cpp
	src/uinull.cpp
	src/videonull.cpp
  )
else ()
  set (PUREIKYUBU_FRONTEND
	src/bbaudp.cpp
	src/audiosdl.cpp
	src/bench.cpp
	src/debugui2.cpp
	src/debugui2gl.cpp
	src/padsdl.cpp
	src/uisdl.cpp
	src/uisettings.cpp
	src/uisettingsgba.cpp
	src/uitheme.cpp
	src/videosdl.cpp

	# The SDL frontend of the portable machines and their debug interface. The two are the only
	# files of the module that belong to a frontend: `gba_sdl.cpp` is the `--gba` mode's window and
	# `gba_debug.cpp` speaks JDI and Markdown (its headers need the third-party paths the core does
	# not have). The core itself is in `PUREIKYUBU_GBA` below, because the Game Boy Player device
	# (gbplayer.cpp) drives a Game Boy Advance in every build.
	src/gba/gba_debug.cpp
	src/gba/gba_sdl.cpp

	thirdparty/imgui/imgui.cpp
	thirdparty/imgui/imgui_demo.cpp
	thirdparty/imgui/imgui_draw.cpp
	thirdparty/imgui/backends/imgui_impl_sdl2.cpp
	thirdparty/imgui/backends/imgui_impl_sdlrenderer2.cpp
	thirdparty/imgui/imgui_tables.cpp
	thirdparty/imgui/imgui_widgets.cpp
  )
endif ()

# The integrated GBA/Game Boy emulator (issue #388). The core is self contained (see
# src/gba/Readme.md) and needs no SDL, so both builds link it: the headless one has no `--gba` mode,
# but the Game Boy Player device of the peripheral pool runs a Game Boy Advance of its own, and that
# machine is the same core.

set (PUREIKYUBU_GBA
	src/gba/arm7tdmi.cpp
	src/gba/gba.cpp
	src/gba/gba_apu.cpp
	src/gba/gba_armasm.cpp
	src/gba/gba_bootrom.cpp
	src/gba/gba_bus.cpp
	src/gba/gba_cart.cpp
	src/gba/gba_disasm.cpp
	src/gba/gba_dma.cpp
	src/gba/gba_hlebios.cpp
	src/gba/gba_irq.cpp
	src/gba/gba_keypad.cpp
	src/gba/gba_ppu.cpp
	src/gba/gba_savestate.cpp
	src/gba/gba_settings.cpp
	src/gba/gba_sio.cpp
	src/gba/gba_timers.cpp
	src/gba/gba_types.cpp

	# The Game Boy (DMG/CGB) machine, the second half of the same module: the GBA can run Game Boy
	# cartridges only through its compatibility hardware, which this emulator implements as its own
	# machine (see wiki/gba.md).
	src/gba/gb.cpp
	src/gba/gb_asm.cpp
	src/gba/gb_apu.cpp
	src/gba/gb_bootrom.cpp
	src/gba/gb_bus.cpp
	src/gba/gb_cart.cpp
	src/gba/gb_cpu.cpp
	src/gba/gb_disasm.cpp
	src/gba/gb_ppu.cpp
	src/gba/gb_savestate.cpp
)

add_executable (pureikyubu
	${PUREIKYUBU_CORE}
	${PUREIKYUBU_GBA}
	${PUREIKYUBU_FRONTEND}
)

target_compile_features(pureikyubu PRIVATE cxx_std_17)

# `src` is on the include path because the debug interface of the portable machines
# (`src/gba/gba_debug.cpp`) includes the emulator's precompiled header the way the MSVC project
# declares it, and because the sources of the portable module include their own headers by name.
target_include_directories( src thirdparty/imgui thirdparty/stb thirdparty/zstd ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} $(GLEW_INCLUDE_DIRS) )

if (APPLE)
  # Shared headers still use _LINUX to select their POSIX implementation.
  target_link_libraries(pureikyubu PRIVATE pureikyubu_glew)
  if (HEADLESS)
    target_include_directories(pureikyubu PRIVATE thirdparty/SDL2/include)
  endif ()
endif ()

if (HEADLESS)
  # The headless build has no GL context and no window, so it neither needs nor wants the SDL,
  # OpenGL and GLEW libraries; only their headers are still read by the shared sources.
  target_link_libraries (pureikyubu PRIVATE Threads::Threads)
else ()
  if (APPLE)
    target_link_libraries(pureikyubu PRIVATE Threads::Threads OpenGL::GL SDL2::SDL2-static)
  else ()
    target_link_libraries(pureikyubu PRIVATE Threads::Threads ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} SDL2 GLEW)
  endif ()
endif ()
