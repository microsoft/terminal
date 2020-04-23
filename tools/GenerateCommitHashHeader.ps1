# This script gets the current git commit hash and places it in a header file stamped to a wstring_view

Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -NoNewline
git rev-parse HEAD  | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
