if(-not (Get-Command "pwsh.exe" -ErrorAction SilentlyContinue)){
    Write-Error "Incorrect version of PowerShell installed.`nMake sure you have at least PowerShell Core 7.0.0."
}