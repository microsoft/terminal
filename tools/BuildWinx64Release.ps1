Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild /p:Configuration=Release /p:Platform=x64 /p:AdditionalOptions="/Zm1000" /maxcpucount:10
Invoke-OpenConsoleBuild /p:Configuration=Release /p:Platform=x64 /p:AdditionalOptions="/Zm1000" /t:Terminal\CascadiaPackage /m
