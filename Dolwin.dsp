# Microsoft Developer Studio Project File - Name="Dolwin" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Dolwin - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Dolwin.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Dolwin.mak" CFG="Dolwin - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Dolwin - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Dolwin - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\SRC" /I ".\SRC\Emulator" /I ".\SRC\UserMenu" /I ".\SRC\Hardware" /I ".\SRC\HighLevel" /I ".\RES" /I ".\DVD\SRC" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "__MSVC__" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x400000" /subsystem:windows /machine:I386 /out:"Dolwin.exe"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".\SRC" /I ".\SRC\Emulator" /I ".\SRC\UserMenu" /I ".\SRC\Hardware" /I ".\SRC\HighLevel" /I ".\RES" /I ".\DVD\SRC" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "__MSVC__" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib comctl32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x400000" /subsystem:windows /debug /machine:I386 /out:"Dolwin.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Dolwin - Win32 Release"
# Name "Dolwin - Win32 Debug"
# Begin Group "Emulator"

# PROP Default_Filter ""
# Begin Group "Debugger"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\break.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\break.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\cmd.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\cmd.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\console.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\console.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\cpuview.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\cpuview.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\input.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\input.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\memview.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\memview.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\output.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\output.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\regs.cpp

!IF  "$(CFG)" == "Dolwin - Win32 Release"

# PROP Intermediate_Dir "Release\Debugger"

!ELSEIF  "$(CFG)" == "Dolwin - Win32 Debug"

# PROP Intermediate_Dir "Debug\Debugger"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger\regs.h
# End Source File
# End Group
# Begin Group "Interpreter"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_branch.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_branch.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_compare.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_compare.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_condition.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_condition.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_floatingpoint.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_floatingpoint.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_fploadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_fploadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_integer.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_integer.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_loadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_loadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_logical.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_logical.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_pairedsingle.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_pairedsingle.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_psloadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_psloadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_rotate.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_rotate.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_shift.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_shift.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_system.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_system.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_tables.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter\c_tables.h
# End Source File
# End Group
# Begin Group "Recompiler"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_branch.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_branch.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_compare.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_compare.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_condition.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_condition.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_floatingpoint.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_floatingpoint.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_fploadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_fploadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_integer.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_integer.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_loadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_loadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_logical.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_logical.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_pairedsingle.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_pairedsingle.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_psloadstore.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_psloadstore.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_rotate.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_rotate.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_shift.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_shift.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_system.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_system.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_tables.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\a_tables.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler\X86.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\SRC\Emulator\Compare.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Compare.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Debugger.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\DisasmPPC.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\DisasmPPC.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\DisasmX86.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\DisasmX86.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Emulator.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Emulator.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Gekko.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Gekko.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Interpreter.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Loader.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Loader.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Memory.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Memory.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\Recompiler.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\SaveLoad.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Emulator\SaveLoad.h
# End Source File
# End Group
# Begin Group "User Menu"

# PROP Default_Filter ""
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\RES\Dolwin.ico
# End Source File
# Begin Source File

SOURCE=.\RES\Dolwin.manifest
# End Source File
# Begin Source File

SOURCE=.\RES\Dolwin.rc
# End Source File
# Begin Source File

SOURCE=.\RES\GCN.ico
# End Source File
# Begin Source File

SOURCE=.\RES\GCN_sm.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon1.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon2.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon3.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon4.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon5.ico
# End Source File
# Begin Source File

SOURCE=.\RES\moreicon6.ico
# End Source File
# Begin Source File

SOURCE=.\RES\nobanner.h
# End Source File
# Begin Source File

SOURCE=.\RES\resource.h
# End Source File
# Begin Source File

SOURCE=.\RES\SjisTable.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\SRC\UserMenu\User.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserAbout.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserAbout.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserConfig.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserConfig.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserFile.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserFile.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserFonts.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserFonts.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserMain.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserMain.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserMemcards.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserMemcards.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserPlugins.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserPlugins.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserProfiler.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserProfiler.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserSelector.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserSelector.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserSettings.h
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\UserMenu\UserWindow.h
# End Source File
# End Group
# Begin Group "GCN Hardware"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\Hardware\AI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\AI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\AR.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\AR.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\CP.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\CP.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\DI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\DI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\DolwinPluginSpecs.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\DolwinPluginSpecs2.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\EFB.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\EFB.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\EI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\EI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\GDI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\GDI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\Hardware.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\HW.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\HW.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\MC.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\MC.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\MI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\MI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\PI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\PI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\Plugins.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\Plugins.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\SI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\SI.h
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\VI.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\Hardware\VI.h
# End Source File
# End Group
# Begin Group "GCN High Level"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\SRC\HighLevel\AX.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\AX.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Bootrom.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Bootrom.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\DSP.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\DSP.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\DVDBanner.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\DVDBanner.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\HighLevel.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\HLE.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\HLE.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\JAudio.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\JAudio.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapLoader.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapMaker.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapMaker.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapSaver.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\MapSaver.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Mtx.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Mtx.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\OS.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\OS.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\PAD.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\PAD.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Stdc.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Stdc.h
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Symbols.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\HighLevel\Symbols.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\SRC\dolphin.h
# End Source File
# End Target
# End Project
