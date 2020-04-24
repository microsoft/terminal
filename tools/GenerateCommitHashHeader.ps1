# This script gets the current git commit hash and places it in a header file stamped to a wstring_view

Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath "test.txt" -Encoding ASCII -NoNewline

$latestMasterHash = (git ls-remote https://github.com/microsoft/terminal.git --heads master).Split()[0]

$hash = "temp"
$branchName = git branch --show-current
if ($LASTEXITCODE -or $branchName -eq "")
{
    # If for whatever reason we're not in our git repo or we're in detached HEAD,
    # just use the remote master's latest commit.
    $hash = $latestMasterHash
}
else
{
    # So now we should try to check that this branch even exists in remote
    # If it does, we'll return the latest commit of this branch in remote
    # Otherwise, we'll return remote master's latest commit.
    $currentBranchHash = git ls-remote https://github.com/microsoft/terminal.git --heads $branchName
    if ($LASTEXITCODE -or $currentBranchHash -eq "")
    {
        $hash = $latestMasterHash
    }
    else 
    {
        $hash = $currentBranchHash.Split()[0]
    }
}

$hash | Out-File -FilePath "test.txt" -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath "test.txt" -Encoding ASCII -Append -NoNewline
