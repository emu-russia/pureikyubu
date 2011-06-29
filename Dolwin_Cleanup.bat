@echo off
@echo תתתתתתת Clean-up Dolwin (remove all object/temporary files)

@echo תתתתתתת Removing executables..
del /Q .\Dolwin.exe
del /Q .\DolwinD.exe
del /Q .\GCMCMPR.exe

@echo תתתתתתת Removing plugins..
del /Q .\Plugins\DVDDefault.ilk
del /Q .\Plugins\DVDDefault.dll
del /Q .\Plugins\PADDefault.ilk
del /Q .\Plugins\PADDefault.dll

@echo תתתתתתת Removing CodeWarrior Data
rd /S /Q .\Dolwin_Data

@echo תתתתתתת Removing Dolwin object and temporary files
rd /S /Q .\Debug
rd /S /Q .\Release
del /Q .\Dolwin.plg
del /Q .\Dolwin.opt
del /Q .\Dolwin.ncb
del /Q .\Dolwin.ilk
del /Q .\DolwinD.ilk
del /Q .\RES\Dolwin.aps

@echo תתתתתתת Removing Plugins object and temporary files
del /Q .\DVD\DVD.plg
rd /S /Q .\DVD\Debug
rd /S /Q .\DVD\Release
del /Q .\DVD\RES\DVD.aps
del /Q .\PAD\PAD.plg
rd /S /Q .\PAD\Debug
rd /S /Q .\PAD\Release
del /Q .\PAD\RES\PAD.aps
del /Q .\GCMCMPR\GCMCMPR.plg
del /Q .\GCMCMPR.ilk
rd /S /Q .\GCMCMPR\Debug
rd /S /Q .\GCMCMPR\Release

@echo תתתתתתת Now rebuild Dolwin..
