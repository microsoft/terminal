# This script is used for taking a json file and stamping it into a header with
# the contents of that json files as a constexpr string_view in the header.

param (
    [parameter(Mandatory = $true)]
    [string]$JsonFile,

    [parameter(Mandatory = $true)]
    [string]$OutPath,

    [parameter(Mandatory = $true)]
    [string]$VariableName
)

$fullPath = Resolve-Path $JsonFile
$jsonData = Get-Content -Raw $JsonFile | ConvertFrom-Json | ConvertTo-Json -Compress -Depth 100

@(
    "// Copyright (c) Microsoft Corporation",
    "// Licensed under the MIT license.",
    "",
    "// THIS IS AN AUTO-GENERATED FILE",
    "// Generated from $($fullPath.Path)",
    "constexpr std::string_view $VariableName{ R`"#($jsonData)#`" };"
) | Out-File -FilePath $OutPath -Encoding utf8
