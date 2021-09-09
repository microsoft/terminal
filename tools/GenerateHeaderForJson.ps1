# This script is used for taking a json file and stamping it into a header with
# the contents of that json files as a constexpr string_view in the header.

param (
    [parameter(Mandatory = $true)]
    [string]$JsonFile,

    [parameter(Mandatory = $true)]
    [string]$OutPath,

    [parameter(Mandatory = $true)]
    [string]$VariableName,

    [switch]$Compress = $false
)

$PSDefaultParameterValues['Out-File:NoNewline'] = $true

$fullPath = Resolve-Path $JsonFile


$out =
    "// Copyright (c) Microsoft Corporation`n" +
    "// Licensed under the MIT license.`n" +
    "`n" +
    "// THIS IS AN AUTO-GENERATED FILE`n" +
    "// Generated from $($fullPath.Path)`n" +
    "constexpr std::string_view $VariableName{"

if ($Compress) {
    $out += "R`"#("
    $out += Get-Content -Raw $JsonFile | ConvertFrom-Json | ConvertTo-Json -Compress -Depth 100
    $out += ")#`""
} else {
    $out += "`n"
    Get-Content $JsonFile | ForEach-Object {
        $out += "R`"#($_)#`" `"\n`"`n"
    }
}

$out += "};`n"

Set-Content -Path $OutPath -Value $out -Encoding utf8 -NoNewline
