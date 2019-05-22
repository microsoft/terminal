@echo off

setlocal
set _last_build=%OPENCON%\bin\%ARCH%\%_LAST_BUILD_CONF%

if not exist %_last_build%\OpenConsole.exe (
    echo Could not locate the OpenConsole.exe in %_last_build%. Double check that it has been built and try again.
    goto :eof
)
if not exist %_last_build%\conechokey.exe (
    echo Could not locate the conechokey.exe in %_last_build%. Double check that it has been built and try again.
    goto :eof
)

rem %_last_build%\conechokey.exe %*

set _r=%random%
set copy_dir=OpenConsole\%_r%
rem Generate a unique name, so that we can debug multiple revisions of the binary at the same time if needed.

(xcopy /Y %_last_build%\OpenConsole.exe %TEMP%\%copy_dir%\OpenConsole.exe*) > nul
(xcopy /Y %_last_build%\conechokey.exe %TEMP%\%copy_dir%\conechokey.exe*) > nul

rem start %TEMP%\%copy_dir%\OpenConsole.exe %TEMP%\%copy_dir%\conechokey.exe %*
%TEMP%\%copy_dir%\conechokey.exe %*
