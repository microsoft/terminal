
#.SYNOPSIS
# Checks for code formatting errors. Will throw exception if any are found.
function Invoke-CheckBadCodeFormatting() {
    Import-Module ./tools/OpenConsole.psm1

    # Don't run the XAML formatter in this step - even if it changes nothing,
    # it'll still touch all the .xaml files.
    Invoke-CodeFormat -IgnoreXaml

    # returns a non-zero exit code if there are any diffs in the tracked files in the repo
    git diff-index --quiet HEAD --
    if ($LASTEXITCODE -eq 1) {

        # Write the list of files that need updating to the log
        git diff-index --name-only HEAD | ForEach-Object {
            $line = 1
            $diffHeader = & git diff HEAD -- "$_" | Select-String "^@@" | Select -First 1 -Expand Line
            If ($diffHeader -Match "\+(\d+)") {
                $line = [int]$Matches[1]
            }
            Write-Host ("::error file={0},line={1},title=Format::{0} is improperly formatted" -f ($_, $line))
        }

        throw "code formatting bad, run Invoke-CodeFormat on branch"
    }

    # Manually check the formatting of our .xaml files, without touching them.
    Test-XamlFormat

}

Invoke-CheckBadCodeFormatting
