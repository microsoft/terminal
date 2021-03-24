
#.SYNOPSIS
# Checks for code formatting errors. Will throw exception if any are found.
function Invoke-CheckBadCodeFormatting() {
    Import-Module ./tools/OpenConsole.psm1
    Invoke-CodeFormat

    # returns a non-zero exit code if there are any diffs in the tracked files in the repo
    git diff-index HEAD --
    if ($lastExitCode -eq 1) {

        git --no-pager diff HEAD

        # Write the list of files that need updating to the log
        git diff-index --name-only HEAD


        throw "code formatting bad, run Invoke-CodeFormat on branch"
    }
}

Invoke-CheckBadCodeFormatting
