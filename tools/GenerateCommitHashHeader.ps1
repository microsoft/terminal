# This script gets the current git commit hash and places it in a header file stamped to a wstring_view
# If anything goes wrong attempting to retrieve the hash, we'll just return the hardcoded string "master" instead.

Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -NoNewline

# Let's see if we're on a build agent that can give us the commit hash.
$hash = $env:Build.SourceVersion
if ($null -eq $hash)
{
    $hash = git rev-parse HEAD
    if ($LASTEXITCODE -or $null -eq $hash)
    {
        $hash = "master"
    }
}

$hash | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
