[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$SearchDir,
    [Parameter(Mandatory=$true, Position=1)][string]$SourceRoot,
    [Parameter(Mandatory=$true, Position=2)][string]$CommitId,
    [string]$Organization = "microsoft",
    [string]$Repo = "terminal",
    [switch]$recursive
)

$debuggerPath = (Get-ItemProperty -path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots" -name WindowsDebuggersRoot10).WindowsDebuggersRoot10
$srcsrvPath = Join-Path $debuggerPath "x64\srcsrv"
$srctoolExe = Join-Path $srcsrvPath "srctool.exe"
$pdbstrExe = Join-Path $srcsrvPath "pdbstr.exe"

$fileTable = @{}
foreach ($gitFile in & git ls-files)
{
    $fileTable[$gitFile] = $gitFile
}

$mappedFiles = New-Object System.Collections.ArrayList

foreach ($file in (Get-ChildItem -r:$recursive "$SearchDir\*.pdb"))
{
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
                $mappedFiles.Add($mapping)

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

    & $pdbstrExe -p:"$file" -w -s:srcsrv -i:$pdbstrFile
}

# Return with exit 0 to override any weird error code from other tools
Exit 0