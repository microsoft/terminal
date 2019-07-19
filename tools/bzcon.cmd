@echo off

"%msbuild%" Openconsole.sln /t:Conhost\Host_EXE /m /p:Configuration=Debug /p:Platform=%ARCH%

:eof
