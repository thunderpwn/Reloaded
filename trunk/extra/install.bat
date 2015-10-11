REM install.bat - acqus personal windows install script to install the whole mod in two directories.
REM if you want to use it you must adjust the path below in the config section and you must have 7-Zip program and configured it in sytempath.

REM -- CONFIG START
set PATH_TO_WETEXE_1=C:\Users\DerpMan\WET1
set PATH_TO_WETEXE_2=C:\Users\DerpMan\WET2
set MODNAME=etrel
set PATH_TO_ETMAIN=..\..\etmain
set PATH_TO_BINS=.
REM -- CONFIG END

7z.exe u -tzip etrel.zip %PATH_TO_BINS%\cgame_mp_x86.dll %PATH_TO_BINS%\ui_mp_x86.dll %PATH_TO_ETMAIN%\animations %PATH_TO_ETMAIN%\fonts %PATH_TO_ETMAIN%\gfx %PATH_TO_ETMAIN%\hud %PATH_TO_ETMAIN%\maps %PATH_TO_ETMAIN%\models %PATH_TO_ETMAIN%\scripts %PATH_TO_ETMAIN%\sound %PATH_TO_ETMAIN%\textures %PATH_TO_ETMAIN%\ui %PATH_TO_ETMAIN%\watermark %PATH_TO_ETMAIN%\weapons -r
rename etrel.zip etrel.pk3
copy etrel.pk3 %PATH_TO_WETEXE_1%\%MODNAME%
copy %PATH_TO_BINS%\cgame_mp_x86.dll %PATH_TO_WETEXE_1%\%MODNAME%
copy %PATH_TO_BINS%\qagame_mp_x86.dll %PATH_TO_WETEXE_1%\%MODNAME%
copy %PATH_TO_BINS%\ui_mp_x86.dll %PATH_TO_WETEXE_1%\%MODNAME%
copy etrel.pk3 %PATH_TO_WETEXE_2%\%MODNAME%
copy %PATH_TO_BINS%\cgame_mp_x86.dll %PATH_TO_WETEXE_2%\%MODNAME%
copy %PATH_TO_BINS%\qagame_mp_x86.dll %PATH_TO_WETEXE_2%\%MODNAME%
copy %PATH_TO_BINS%\ui_mp_x86.dll %PATH_TO_WETEXE_2%\%MODNAME%

del etrel.pk3