@echo off
wcl -q -bt=dos -ms -0 -w4 -fe=ELPDS.EXE ELPDS.CPP > BUILD.LOG
if errorlevel 1 goto failed
echo BUILD OK > BUILD.OK
goto done
:failed
echo BUILD FAILED
:done
