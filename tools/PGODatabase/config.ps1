$pgoBranch           = "main"
$packageId           = "Microsoft.Internal.Windows.Terminal.PGODatabase"

# Get release version
[xml] $customProps   = ( Get-Content "..\..\custom.props" )
$releaseVersionMajor = ( [int]::Parse( $customProps.GetElementsByTagName("VersionMajor").'#text' ) )
$releaseVersionMinor = ( [int]::Parse( $customProps.GetElementsByTagName("VersionMinor").'#text' ) )