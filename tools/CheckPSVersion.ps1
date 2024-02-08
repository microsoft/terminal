if($PSVersionTable.PSVersion.Major -lt 7){
   Write-Error "Incorrect version of PowerShell installed.`nMake sure you have at least PowerShell Core 7.0.0."
   Exit 1 
}