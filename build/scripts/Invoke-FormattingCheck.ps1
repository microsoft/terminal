
#.SYNOPSIS
# Checks for code formatting errors. Will throw exception if any are found.
function Invoke-CheckBadCodeFormatting() {
    Import-Module ./tools/OpenConsole.psm1

    # Don't run the XAML formatter in this step - even if it changes nothing,
    # it'll still touch all the .xaml files.
    Invoke-CodeFormat -IgnoreXaml

    $file = $null
    $count = 0
    git -c core.quotePath=false diff --no-color -U0 HEAD | ForEach-Object {
        if ($_ -cmatch '^\+\+\+ b/(.*)$')
        {
            $file = $Matches[1]
            $count++
        }
        elseif ($null -ne $file -and $_ -cmatch '^@@ .* \+(\d+)')
        {
            $line = [int]$Matches[1]
            Write-Host ("::error file={0},line={1},title=Format::{0} is improperly formatted" -f ($file, $line))
            $file = $null
        }
    }

    if ($count -gt 0) {
        throw "code formatting bad, run Invoke-CodeFormat on branch"
    }

    # Manually check the formatting of our .xaml files, without touching them.
    Test-XamlFormat

}

Invoke-CheckBadCodeFormatting
