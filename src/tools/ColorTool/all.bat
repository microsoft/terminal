@echo off

rem all.bat
rem     This tool can be used to iterate over all the schemes you have installed 
rem     To help find one that you like. Simply press Ctrl+C when you get to one you like.
rem     Note: You will likely destroy your current console window's history.
rem     Only the most recent theme is visible in the console. 
rem     All of the previously viewed tables will display the current scheme's colors.

for %%i in (schemes\*) do (
    echo %%i
    .\colortool.exe "%%i"
    pause
)
