# This script is used for taking a json file and stripping the whitespace from it.

param (
    [parameter(Mandatory = $true)]
    [string]$JsonFile,

    [parameter(Mandatory = $true)]
    [string]$OutPath
)

$jsonData = Get-Content -Raw $JsonFile | ConvertFrom-Json | ConvertTo-Json -Compress -Depth 100

$jsonData | Out-File -FilePath $OutPath -Encoding utf8
