Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild /p:Configuration=Release /p:Platform=x64 /p:AdditionalOptions="/Zm2000"
Invoke-OpenConsoleBuild /p:Configuration=Release /p:Platform=x64 /p:AdditionalOptions="/Zm2000" /t:Terminal\CascadiaPackage /m
