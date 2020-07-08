[CmdletBinding()]
Param(
    [Parameter(Position=0, Mandatory=$true)][string]$MarkdownNoticePath,
    [Parameter(Position=1, Mandatory=$true)][string]$OutputPath
)

@"
<html>
  <head><title>Third-Party Notices</title></head>
  <body>
  $(ConvertFrom-Markdown $MarkdownNoticePath | Select -Expand Html)
  </body>
</html>
"@ | Out-File -Encoding UTF-8 $OutputPath -Force
