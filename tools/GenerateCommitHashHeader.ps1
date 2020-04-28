# This script gets the current git commit hash and places it in a header file stamped to a wstring_view
# If we're unable to retrieve the hash, we'll return the hard coded string "master" instead.

$filePath = "Generated Files\CurrentCommitHash.h"

Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath $filePath -Encoding ASCII -NoNewline

# Let's see if we're on a build agent that can give us the commit hash through this predefined variable.
$hash = $env:Build_SourceVersion
if (-not $hash)
{
    # Otherwise attempt to invoke git to get the commit.
    $hash = git rev-parse HEAD
    if ($LASTEXITCODE -or -not $hash)
    {
        $hash = "master"
    }
}

$hash | Out-File -FilePath $filePath -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath $filePath -Encoding ASCII -Append -NoNewline
