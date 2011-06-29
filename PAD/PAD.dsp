# Microsoft Developer Studio Project File - Name="PAD" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=PAD - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PAD.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PAD.mak" CFG="PAD - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PAD - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "PAD - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "PAD - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PAD_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".\SRC" /I ".\RES" /I "..\SRC" /I "..\SRC\UserMenu" /I "..\SRC\HighLevel" /I "..\SRC\Hardware" /I "..\SRC\Emulator" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PAD_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"../Plugins/PADDefault.dll"

!ELSEIF  "$(CFG)" == "PAD - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PAD_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".\SRC" /I ".\RES" /I "..\SRC" /I "..\SRC\UserMenu" /I "..\SRC\HighLevel" /I "..\SRC\Hardware" /I "..\SRC\Emulator" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PAD_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../Plugins/PADDefault.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "PAD - Win32 Release"
# Name "PAD - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\SRC\Configure.cpp
# End Source File
# Begin Source File

SOURCE=.\SRC\PAD.cpp
# End Source File
# Begin Source File

SOURCE=..\SRC\UserMenu\UserConfig.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\SRC\Configure.h
# End Source File
# Begin Source File

SOURCE=..\SRC\Hardware\DolwinPluginSpecs.h
# End Source File
# Begin Source File

SOURCE=.\SRC\PAD.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;rc;def;"
# Begin Source File

SOURCE=.\RES\PAD.def
# End Source File
# Begin Source File

SOURCE=.\RES\PAD.ico
# End Source File
# Begin Source File

SOURCE=.\RES\PAD.rc
# End Source File
# Begin Source File

SOURCE=.\RES\padArrows.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padButtons.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padStart.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padStick.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padSubStick.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padTriggerL.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\padTriggerR.bmp
# End Source File
# Begin Source File

SOURCE=.\RES\resource.h
# End Source File
# End Group
# End Target
# End Project
