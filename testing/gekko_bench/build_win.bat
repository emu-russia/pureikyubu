@echo off
rem Build the standalone Gekko benchmark with MSVC, from the current working tree.
rem This is the Windows counterpart of build.sh: the same sources and the same stubs,
rem so that the two compilers can be compared on exactly the same workload.
rem
rem   BLD   scratch directory (default <here>\build_win)
rem   VCVARS  path to vcvars64.bat (default: Visual Studio 2022/2026 Community)
setlocal
set HERE=%~dp0
if "%BLD%"=="" set BLD=%HERE%build_win
if "%VCVARS%"=="" set VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul

set REPO=%HERE%..\..
if not exist "%BLD%\src" mkdir "%BLD%\src"
copy /y "%REPO%\src\gekko.cpp"        "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkoc.cpp"       "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkodec.cpp"     "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkodisasm.cpp"  "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkojit.cpp"     "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkojit_ps.cpp"  "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkojit_x64.h"   "%BLD%\src\" >nul
copy /y "%REPO%\src\gekkojit_ps.h"    "%BLD%\src\" >nul
copy /y "%REPO%\src\gqr.h"            "%BLD%\src\" >nul

pushd "%HERE%"
cl /nologo /O2 /Oi /GL /EHsc /std:c++17 /MT /W3 /wd4996 /DBENCH_WITH_JIT /D_WINDOWS ^
   /I. /I"%BLD%\src" /I"%REPO%\src" ^
   "%BLD%\src\gekko.cpp" "%BLD%\src\gekkoc.cpp" "%BLD%\src\gekkodec.cpp" ^
   "%BLD%\src\gekkojit.cpp" "%BLD%\src\gekkojit_ps.cpp" "%BLD%\src\gekkodisasm.cpp" ^
   stubs.cpp bench.cpp bench_win_prof.cpp ^
   /Fe:"%BLD%\bench.exe" /Fo:"%BLD%\\" /link /LTCG
popd
