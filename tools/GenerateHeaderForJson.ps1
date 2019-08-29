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

Write-Output "// Copyright (c) Microsoft Corporation" | Out-File -FilePath -Encoding ASCII $OutPath
Write-Output "// Licensed under the MIT license." | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output "" | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output "// THIS IS AN AUTO-GENERATED FILE" | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output "// Generated from " | Out-File -FilePath -Encoding ASCII $OutPath -Append -NoNewline
$fullPath = Resolve-Path -Path $JsonFile
Write-Output $fullPath.Path | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output "constexpr std::string_view $($VariableName){ R`"(" | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output $jsonData | Out-File -FilePath -Encoding ASCII $OutPath -Append
Write-Output "})`" };" | Out-File -FilePath -Encoding ASCII $OutPath -Append

