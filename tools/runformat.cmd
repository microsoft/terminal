@echo off

rem run clang-format on c++ files

powershell -noprofile "import-module %OPENCON_TOOLS%\openconsole.psm1; Invoke-CodeFormat"
