# This script gets the current git commit hash and places it in a header file stamped to a wstring_view
# If literally anything goes wrong in this script with regards to git, like not being in a git repo,
# not being able to find the current branch on remote, not being able to figure out what branch you're on,
# we'll just return the latest commit hash of remote master.

Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -NoNewline

$latestMasterHash = (git ls-remote https://github.com/microsoft/terminal.git --heads master).Split()[0]

$hash = $latestMasterHash
$branchName = git branch --show-current
if (-not $LASTEXITCODE -and $branchName -ne $null)
{
    $currentBranchHash = git ls-remote https://github.com/microsoft/terminal.git --heads $branchName
    if (-not $LASTEXITCODE -and $currentBranchHash -ne $null)
    {
        $hash = $currentBranchHash.Split()[0]
    }
}

$hash | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
