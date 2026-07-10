param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86", "arm64", "arm")]
    [string]$Platform = "x64",

    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$appxRoot = Join-Path $repoRoot "src\cascadia\CascadiaPackage\bin\$Platform\$Configuration"
$manifest = Join-Path $appxRoot "AppxManifest.xml"

if (-not (Test-Path -LiteralPath $manifest)) {
    throw "AppX manifest not found: $manifest. Build Terminal\CascadiaPackage for $Configuration|$Platform first."
}

Write-Host "Registering WindowsTerminalDev from:"
Write-Host "  $appxRoot"

# A development package cannot be moved to another loose-layout directory by
# registering the new manifest over the old one. Remove only this dev package
# when its existing registration points somewhere else (for example, a stale
# nested AppX directory from an older build configuration).
$existingPackage = Get-AppxPackage -Name "WindowsTerminalDev" -ErrorAction SilentlyContinue
if ($existingPackage) {
    Write-Host "Removing existing WindowsTerminalDev registration from:"
    Write-Host "  $($existingPackage.InstallLocation)"
    $existingPackage | Remove-AppxPackage
}

Add-AppxPackage -Register $manifest -ForceApplicationShutdown

$alias = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps\wtd.exe"
if (-not (Test-Path -LiteralPath $alias)) {
    throw "wtd.exe alias was not created at $alias. Check Windows App Execution Aliases settings."
}

Write-Host "wtd alias:"
Write-Host "  $alias"

if (-not $NoLaunch) {
    Write-Host "Launching wtd..."
    Start-Process -FilePath $alias
}
