@echo off

rem run xstyler on xaml files

powershell -noprofile "import-module %OPENCON_TOOLS%\openconsole.psm1; Invoke-XamlFormat"
