[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$SearchDir,
    [Parameter(Mandatory=$true, Position=1)][string]$SourceRoot,
    [Parameter(Mandatory=$true, Position=2)][string]$CommitId,
    [string]$Organization = "microsoft",
    [string]$Repo = "terminal",
    [switch]$recursive
)

$pdbStrPackage = ([xml](Get-Content "$SourceRoot\build\packages.config")).packages.package | Where-Object id -like "*PdbStr*"
# This assumes that we rev PdbStr and SrcTool at the same time.
$debugPackageVersions = $pdbStrPackage.version
$srctoolExe = Join-Path $SourceRoot "packages" "Microsoft.Debugging.Tools.SrcTool.$debugPackageVersions" "content" "amd64" "srctool.exe"
$pdbstrExe = Join-Path $SourceRoot "packages" "Microsoft.Debugging.Tools.PdbStr.$debugPackageVersions" "content" "amd64" "pdbstr.exe"

$fileTable = @{}
foreach ($gitFile in & git ls-files)
{
    $fileTable[$gitFile] = $gitFile
}

$mappedFiles = New-Object System.Collections.ArrayList

foreach ($file in (Get-ChildItem -r:$recursive "$SearchDir\*.pdb"))
{
    $mappedFiles = New-Object System.Collections.ArrayList
    Write-Verbose "Found $file"

    $ErrorActionPreference = "Continue" # Azure Pipelines defaults to "Stop", continue past errors in this script.
    
    $allFiles = & $srctoolExe -r "$file"

    # If the pdb didn't have enough files then skip it (the srctool output has a blank line even when there's no info
    # so check for less than 2 lines)
    if ($allFiles.Length -lt 2)
    {
        continue
    }

    for ($i = 0; $i -lt $allFiles.Length; $i++)
    {
        if ($allFiles[$i].StartsWith($SourceRoot, [StringComparison]::OrdinalIgnoreCase))
        {
            $relative = $allFiles[$i].Substring($SourceRoot.Length).TrimStart("\")
            $relative = $relative.Replace("\", "/")

            # Git urls are case-sensitive but the PDB might contain a lowercased version of the file path.
            # Look up the relative url in the output of "ls-files". If it's not there then it's not something
            # in git, so don't index it.
            $relative = $fileTable[$relative]
            if ($relative)
            {
                $mapping = $allFiles[$i] + "*$relative"
                $ignore = $mappedFiles.Add($mapping)

                Write-Verbose "Mapped path $($i): $mapping"
            }
        }
    }

    $pdbstrFile = Join-Path "$env:TEMP" "pdbstr.txt"

    Write-Verbose "pdbstr.txt = $pdbstrFile"

    @"
SRCSRV: ini ------------------------------------------------
VERSION=2
VERCTRL=http
SRCSRV: variables ------------------------------------------
ORGANIZATION=$Organization
REPO=$Repo
COMMITID=$CommitId
HTTP_ALIAS=https://raw.githubusercontent.com/%ORGANIZATION%/%REPO%/%COMMITID%/
HTTP_EXTRACT_TARGET=%HTTP_ALIAS%%var2%
SRCSRVTRG=%HTTP_EXTRACT_TARGET%
SRC_INDEX=public
SRCSRV: source files ---------------------------------------
$($mappedFiles -join "`r`n")
SRCSRV: end ------------------------------------------------
"@ | Set-Content $pdbstrFile

    Write-Host
    Write-Host
    Write-Host (Get-Content $pdbstrFile)
    Write-Host
    Write-Host

    Write-Host "$pdbstrExe -p:""$file"" -w -s:srcsrv -i:$pdbstrFile"
    & $pdbstrExe -p:"$file" -w -s:srcsrv -i:$pdbstrFile
    Write-Host
    Write-Host

    Write-Host "$pdbstrExe -p:""$file"" -r -s:srcsrv"
    & $pdbstrExe -p:"$file" -r -s:srcsrv
    Write-Host
    Write-Host

    Write-Host "$srctoolExe $file"
    & $srctoolExe "$file"
    Write-Host
    Write-Host
}

# Return with exit 0 to override any weird error code from other tools
Exit 0