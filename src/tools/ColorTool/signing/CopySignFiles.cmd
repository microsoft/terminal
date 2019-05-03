REM @echo off
REM This script copies the files to have different paths to be signed and copies them back

if "%2" == "" goto :usage
if "%1" == "sign" goto :sign
if "%1" == "afterSign" goto :afterSign

goto :usage

:sign
pushd "%2"
mkdir tosign
call :checkedCopy ..\b\Release\anycpu\ColorTool.exe tosign\ColorTool.exe

popd
goto :end

:afterSign
pushd "%2"
call :checkedCopy signed\ColorTool.exe ..\b\Release\anycpu\ColorTool.exe

popd
goto :end

:checkedCopy
copy %1 %2
if %errorlevel% NEQ 0 (
    popd
    exit 1
)
exit /b

:usage
echo "Usage: CopySignFiles <sign| afterSign> <root of the repo>"
echo "Will copy the binary release from <root>\Release to be sent to signed"

:end