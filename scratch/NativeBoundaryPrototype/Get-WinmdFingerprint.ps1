# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Reflection.Metadata
$stream = [IO.File]::OpenRead((Resolve-Path -LiteralPath $Path).Path)
$pe = [System.Reflection.PortableExecutable.PEReader]::new($stream)
try
{
    $reader = [System.Reflection.Metadata.PEReaderExtensions]::GetMetadataReader(
        $pe, [System.Reflection.Metadata.MetadataReaderOptions]::None)
    $mvid = $reader.GetGuid($reader.GetModuleDefinition().Mvid).ToByteArray()
    [byte[]]$metadata = $pe.GetMetadata().GetContent()
    $matches = [Collections.Generic.List[int]]::new()
    for ($offset = 0; $offset -le $metadata.Length - $mvid.Length; ++$offset)
    {
        if ($metadata[$offset] -ne $mvid[0]) { continue }
        $equal = $true
        for ($i = 1; $i -lt $mvid.Length; ++$i)
        {
            if ($metadata[$offset + $i] -ne $mvid[$i])
            {
                $equal = $false
                break
            }
        }
        if ($equal) { $matches.Add($offset) }
    }
    if ($matches.Count -ne 1) { throw "Expected one module version ID in $Path, found $($matches.Count)" }
    # mdmerge assigns a fresh module version ID on every invocation.
    [Array]::Clear($metadata, $matches[0], $mvid.Length)
    [pscustomobject]@{
        metadataBytes = $metadata.Length
        sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($metadata))
    }
}
finally
{
    $pe.Dispose()
    $stream.Dispose()
}
