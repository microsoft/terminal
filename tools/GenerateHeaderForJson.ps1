# This script is used for taking a json file and stamping it into a header with
# the contents of that json files as a constexpr string_view in the header.

param (
    [parameter(Mandatory=$true, Position=0)]
    [string]$JsonFile,

    [parameter(Mandatory=$true, Position=1)]
    [string]$OutPath,

    [parameter(Mandatory=$true, Position=2)]
    [string]$VariableName
)

# Load the xml files.
$jsonData = Get-Content $JsonFile

Write-Output "// Copyright (c) Microsoft Corporation" | Out-File -FilePath $OutPath -Encoding ASCII
Write-Output "// Licensed under the MIT license." | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output "" | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output "// THIS IS AN AUTO-GENERATED FILE" | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output "// Generated from " | Out-File -FilePath $OutPath -Encoding ASCII -Append -NoNewline
$fullPath = Resolve-Path -Path $JsonFile
Write-Output $fullPath.Path | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output "constexpr std::string_view $($VariableName){ R`"(" | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output $jsonData | Out-File -FilePath $OutPath -Encoding ASCII -Append
Write-Output ")`" };" | Out-File -FilePath $OutPath -Encoding ASCII -Append

