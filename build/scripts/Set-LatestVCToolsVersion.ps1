$VSInstances = ([xml](& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -include packages -format xml))
$VSPackages = $VSInstances.instances.instance.packages.package
$LatestVCPackage = ($VSPackages | ? { $_.id -eq "Microsoft.VisualCpp.Tools.Core" })
$LatestVCToolsVersion = $LatestVCPackage.version;

$VSRoot = (& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property 'resolvedInstallationPath')
$VCToolsRoot = Join-Path $VSRoot "VC\Tools\MSVC"

# We have observed a few instances where the VC tools package version actually
# differs from the version on the files themselves. We might as well check
# whether the version we just found _actually exists_ before we use it.
# We'll use whichever highest version exists.
#
# Some pool images report a package version (e.g. 14.44.35208) but ship only
# package metadata, not the actual toolchain — the version folder either
# doesn't exist or is empty. Detect both cases by also requiring a non-empty
# `bin` subdirectory (where cl.exe / link.exe live).
$PackageVCToolPath = Join-Path $VCToolsRoot $LatestVCToolsVersion
$PackageVCBinPath = Join-Path $PackageVCToolPath "bin"
$PackageIsValid = ($Null -Ne (Get-Item $PackageVCToolPath -ErrorAction:Ignore)) -And `
                  ($Null -Ne (Get-Item $PackageVCBinPath  -ErrorAction:Ignore))
If (-Not $PackageIsValid) {
    $VCToolsVersions = Get-ChildItem $VCToolsRoot -Directory -ErrorAction:Ignore | Where-Object {
        # Only consider directories that actually contain a populated `bin`.
        $binDir = Join-Path $_.FullName "bin"
        (Test-Path $binDir) -And ((Get-ChildItem $binDir -Recurse -File -ErrorAction:Ignore | Select-Object -First 1) -Ne $Null)
    } | ForEach-Object {
        [Version]$_.Name
    } | Sort -Descending
    $LatestActualVCToolsVersion = $VCToolsVersions | Select -First 1

    If ($Null -Eq $LatestActualVCToolsVersion) {
        Write-Error "No usable VC Tools installation found under $VCToolsRoot. Package version was $LatestVCToolsVersion. This typically indicates a partial/broken pool image (DD-1541167)."
        Exit 1
    }

    If ([Version]$LatestVCToolsVersion -Ne $LatestActualVCToolsVersion) {
        Write-Output "VC Tools Mismatch: Directory = $LatestActualVCToolsVersion, Package = $LatestVCToolsVersion"
        $LatestVCToolsVersion = $LatestActualVCToolsVersion.ToString(3)
    }
}

Write-Output "Latest VCToolsVersion: $LatestVCToolsVersion"
Write-Output "Updating VCToolsVersion environment variable for job"
Write-Output "##vso[task.setvariable variable=VCToolsVersion]$LatestVCToolsVersion"
