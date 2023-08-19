cmake_minimum_required (VERSION 3.0)
project (pureikyubu)

# Choose an X86/X64 build

if (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(X86 OFF)
  set(X64 ON)
else ()
  set(X86 ON)
  set(X64 OFF)
endif ()

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
find_package (Threads)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

add_definitions (-D_LINUX)
add_definitions (-DCMAKE_BUILD_TYPE=Debug)

# ../GekkoCore/Gekko.h:196:66: warning: `fastcall` attribute ignored [-Wattributes]
add_compile_options(-Wno-attributes)

# ThirdParty
add_subdirectory (thirdparty/fmt/Scripts)

add_executable (pureikyubu 
	src/ai.cpp
	src/audionull.cpp
	src/bootrtc.cpp
	src/config.cpp
	src/cp.cpp
	src/cui_imgui.cpp
	src/debug.cpp
	src/debugui.cpp
	src/di.cpp
	src/dsp.cpp
	src/dspcore.cpp
	src/dspdebug.cpp
	src/dspdec.cpp
	src/dspdisasm.cpp
	src/dvd.cpp
	src/dvddebug.cpp
	src/exi.cpp
	src/flipper.cpp
	src/flipperdebug.cpp
	src/gekko.cpp
	src/gekkoc.cpp
	src/gekkodebug.cpp
	src/gekkodec.cpp
	src/gekkodisasm.cpp
	src/gfx.cpp
	src/jdi.cpp
	src/jdiserver.cpp
	src/json.cpp
	src/main.cpp
	src/mem.cpp
	src/memcard.cpp
	src/os.cpp
	src/osdebug.cpp
	src/padnull.cpp
	src/pe.cpp
	src/pi.cpp
	src/ras.cpp
	src/si.cpp
	src/su.cpp
	src/sym.cpp
	src/tev.cpp
	src/tx.cpp
	src/uijdi.cpp
	# If you don't need UI at all, you can use the simple version instead uisdl
	#src/uisimple.cpp
	src/uisdl.cpp
	src/utils.cpp
	src/vi.cpp
	src/xf.cpp
	src/xfbnull.cpp

	thirdparty/imgui/imgui.cpp
	thirdparty/imgui/imgui_demo.cpp
	thirdparty/imgui/imgui_draw.cpp
	thirdparty/imgui/backends/imgui_impl_sdl2.cpp
	thirdparty/imgui/backends/imgui_impl_sdlrenderer2.cpp
	thirdparty/imgui/imgui_tables.cpp
	thirdparty/imgui/imgui_widgets.cpp
)

include_directories( thirdparty/imgui ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} $(GLEW_INCLUDE_DIRS) )

target_link_libraries (pureikyubu ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} fmt SDL2 GLEW)
