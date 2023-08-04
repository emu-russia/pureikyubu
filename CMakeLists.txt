cmake_minimum_required (VERSION 2.8.11)
project (pureikyubu)

# Choose an X86/X64 build

if (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(X86 OFF)
  set(X64 ON)
else ()
  set(X86 ON)
  set(X64 OFF)
endif ()

add_definitions (-D_LINUX)
add_definitions (-DCMAKE_BUILD_TYPE=Debug)

# ../GekkoCore/Gekko.h:196:66: warning: `fastcall` attribute ignored [-Wattributes]
add_compile_options(-Wno-attributes)

# ThirdParty
add_subdirectory (thirdparty/fmt/Scripts)
add_subdirectory (thirdparty/GX/Scripts)

add_executable (pureikyubu 
	src/ai.cpp
	src/audionull.cpp
	src/bootrtc.cpp
	src/config.cpp
	src/cp.cpp
	src/debug.cpp
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
	src/gfxnull.cpp
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
	src/uisimple.cpp
	src/utils.cpp
	src/vi.cpp
	src/xf.cpp
	src/xfbnull.cpp
)

target_link_libraries (pureikyubu LINK_PUBLIC fmt)
target_link_libraries (pureikyubu LINK_PUBLIC GX)
find_package (Threads)
target_link_libraries (pureikyubu ${CMAKE_THREAD_LIBS_INIT})
