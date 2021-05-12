. .\version.ps1
. .\template.ps1
. .\config.ps1

$feedUri = "https://pkgs.dev.azure.com/ms/terminal/_packaging/TerminalDependencies/nuget/v2"

$currentVersion = MakeVersion $releaseVersionMajor $releaseVersionMinor ( GetDatetimeStamp $pgoBranch )

Write-Host ( "PGO OPTIMIZE: requesting {0} version {1}" -f $packageId, ( FormatVersion $currentVersion ) )

$packageSource = Register-PackageSource -ForceBootstrap -Name TerminalDependencies -Location $feedUri -ProviderName NuGet -Trusted
$packages = ( Find-Package $packageId -Source TerminalDependencies -AllowPrereleaseVersions -AllVersions ) | Sort-Object -Property Version -Descending

$best = $null

foreach ( $existing in $packages )
{
    $existingVersion = MakeVersionFromString $existing.Version

    if ( ( CompareBranches $existingVersion $currentVersion ) -eq $False -or
         ( CompareReleases $existingVersion $currentVersion ) -ne 0 )
    {
        # If this is different release or branch, then skip it.
        continue
    }

    if ( ( CompareRevisions $existingVersion $currentVersion ) -le 0 )
    {
        # Version are sorted in descending order, the first one less than or equal to the current is the one we want.
        # NOTE: at this point the only difference between versions will be revision (date-time stamp)
        # which is formatted as a fixed-length string, so string comparison WILL sort it correctly.
        $best = $existing
        break
    }
}

if ( $best -eq $null )
{
    throw "Appropriate database cannot be found"
}

Write-Host ( "PGO OPTIMIZE: picked {0} version {1}" -f $packageId, $best.Version )

$best | Install-Package -Destination ..\..\packages -Force
$packageSource | Unregister-PackageSource

FillOut-Template "PGO.version.props.template" "PGO.version.props" @{ "version" = $best.Version; "id" = $packageId }