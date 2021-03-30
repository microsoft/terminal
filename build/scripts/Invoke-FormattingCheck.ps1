
#.SYNOPSIS
# Checks for code formatting errors. Will throw exception if any are found.
function Invoke-CheckBadCodeFormatting() {
    Import-Module ./tools/OpenConsole.psm1

    # Don't run the XAML formatter in this step - even if it changes nothing,
    # it'll still touch all the .xaml files.
    Invoke-CodeFormat -IgnoreXaml

    # returns a non-zero exit code if there are any diffs in the tracked files in the repo
    git diff-index --quiet HEAD --
    if ($lastExitCode -eq 1) {

        # Write the list of files that need updating to the log
        git diff-index --name-only HEAD

        throw "code formatting bad, run Invoke-CodeFormat on branch"
    }

    # Manually check the formatting of our .xaml files, without touching them.
    Verify-XamlFormat

}

Invoke-CheckBadCodeFormatting
