Param(
  [Parameter(Mandatory = $true, Position = 1)] [string] $templatePath,
  [Parameter(Mandatory = $true, Position = 2)] [string] $outputPath)

. .\version.ps1
. .\template.ps1
. .\config.ps1

$version = FormatVersion ( MakeVersion $releaseVersionMajor $releaseVersionMinor ( GetDatetimeStamp $pgoBranch ) )
Write-Host ( "PGO INSTRUMENT: generating {0} version {1}" -f $packageId, $version )
FillOut-Template $templatePath $outputPath @{ "version" = $version; "id" = $packageId }