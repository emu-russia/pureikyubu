@echo off
@echo Removing all Visual Studio 7+ (.NET) project data..
@echo (to migrate from VC6 .DSW again, with new settings/projects)

@echo Delete VCNET files
del /Q .\Dolwin.sln
del /Q /AH .\Dolwin.suo
del /Q .\Dolwin.vcproj
del /Q .\DVD\DVD.vcproj
del /Q .\PAD\PAD.vcproj
del /Q .\GCMCMPR\GCMCMPR.vcproj

@echo Define __VCNET__ in Project Properties\C/C++\Preprocessor\Definitions

@echo  Clean-up Dolwin
Dolwin_Cleanup.bat
