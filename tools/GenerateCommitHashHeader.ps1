Write-Output "constexpr std::wstring_view CurrentCommitHash{ L`"" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -NoNewline
git rev-parse HEAD  | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline
Write-Output "`" };" | Out-File -FilePath "Generated Files\CurrentCommitHash.h" -Encoding ASCII -Append -NoNewline