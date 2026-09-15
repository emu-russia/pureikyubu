cmake_minimum_required (VERSION 3.0)
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

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
if (NOT HEADLESS)
  find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
endif ()
find_package (Threads)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

add_definitions (-D_LINUX)
#add_definitions (-DCMAKE_BUILD_TYPE=Debug)

if (HEADLESS)
  add_definitions (-DGFX_NULL)
endif ()

# ../GekkoCore/Gekko.h:196:66: warning: `fastcall` attribute ignored [-Wattributes]
add_compile_options(-Wno-attributes)
add_compile_options(-lstdc++fs)

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
	src/cp.cpp
	src/debug.cpp
	src/debugui.cpp
	src/di.cpp
	src/dsp.cpp
	src/dspcore.cpp
	src/dspjit.cpp
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
	src/gekkojit.cpp
	src/gekkojit_ps.cpp
	src/gekkodebug.cpp
	src/gekkodec.cpp
	src/gekkodisasm.cpp
	src/gfx.cpp
	src/gfxosd.cpp
	src/hwosd.cpp
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
	src/pi.cpp
	src/ras.cpp
	src/rvz.cpp
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
	src/bench.cpp
	src/cuinull.cpp
	src/gfxnull.cpp
	src/padnull.cpp
	src/uinull.cpp
	src/videonull.cpp
  )
else ()
  set (PUREIKYUBU_FRONTEND
	src/audiosdl.cpp
	src/bench.cpp
	src/cuisdl.cpp
	src/debugui2.cpp
	src/debugui2gl.cpp
	src/padsdl.cpp
	src/uisdl.cpp
	src/videosdl.cpp

	# The integrated GBA emulator (issue #388). The core is self contained (see src/gba/Readme.md);
	# gba_sdl.cpp is the SDL2 frontend the `--gba` mode uses.
	src/gba/arm7tdmi.cpp
	src/gba/gba.cpp
	src/gba/gba_apu.cpp
	src/gba/gba_armasm.cpp
	src/gba/gba_bootrom.cpp
	src/gba/gba_bus.cpp
	src/gba/gba_cart.cpp
	src/gba/gba_dma.cpp
	src/gba/gba_hlebios.cpp
	src/gba/gba_irq.cpp
	src/gba/gba_keypad.cpp
	src/gba/gba_ppu.cpp
	src/gba/gba_sdl.cpp
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
	src/gba/gb_ppu.cpp

	thirdparty/imgui/imgui.cpp
	thirdparty/imgui/imgui_demo.cpp
	thirdparty/imgui/imgui_draw.cpp
	thirdparty/imgui/backends/imgui_impl_sdl2.cpp
	thirdparty/imgui/backends/imgui_impl_sdlrenderer2.cpp
	thirdparty/imgui/imgui_tables.cpp
	thirdparty/imgui/imgui_widgets.cpp
  )
endif ()

add_executable (pureikyubu
	${PUREIKYUBU_CORE}
	${PUREIKYUBU_FRONTEND}
)

target_compile_features(pureikyubu PRIVATE cxx_std_17)

include_directories( thirdparty/imgui thirdparty/stb thirdparty/zstd ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} $(GLEW_INCLUDE_DIRS) )

if (HEADLESS)
  # The headless build has no GL context and no window, so it neither needs nor wants the SDL,
  # OpenGL and GLEW libraries; only their headers are still read by the shared sources.
  target_link_libraries (pureikyubu ${CMAKE_THREAD_LIBS_INIT})
else ()
  target_link_libraries (pureikyubu ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} SDL2 GLEW)
endif ()
