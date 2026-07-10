param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86", "arm64", "arm")]
    [string]$Platform = "x64",

    [ValidateSet("quiet", "minimal", "normal", "detailed", "diagnostic")]
    [string]$Verbosity = "normal",

    [switch]$Register,
    [switch]$Launch
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$msbuild = (Get-Command msbuild.exe -ErrorAction SilentlyContinue).Source

if (-not $msbuild) {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )

    $msbuild = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not $msbuild) {
    throw "MSBuild.exe was not found. Install Visual Studio with MSBuild, or add MSBuild.exe to PATH."
}

Write-Host "Using MSBuild:"
Write-Host "  $msbuild"
Write-Host "Building WindowsTerminalDev $Configuration|$Platform..."
Write-Host "Verbosity:"
Write-Host "  $Verbosity"

$solution = Join-Path $repoRoot "OpenConsole.slnx"
$log = Join-Path $repoRoot "build-wtd-$($Configuration.ToLowerInvariant())-$Platform.log"
$consoleLogger = if ($Verbosity -eq "quiet") { "/clp:ErrorsOnly" } else { "/clp:Summary" }

& $msbuild $solution `
    "/t:Build;Terminal\CascadiaPackage" `
    "/m" `
    "/p:Configuration=$Configuration" `
    "/p:Platform=$Platform" `
    "/p:GenerateAppxPackageOnBuild=false" `
    "/p:AppxSymbolPackageEnabled=false" `
    $consoleLogger `
    "/v:$Verbosity" `
    "/flp:logfile=$log;verbosity=$Verbosity"

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE. See $log."
}

Write-Host "Build succeeded."
Write-Host "Log:"
Write-Host "  $log"

if ($Register -or $Launch) {
    $registerArgs = @{
        Configuration = $Configuration
        Platform = $Platform
    }

    if (-not $Launch) {
        $registerArgs.NoLaunch = $true
    }

    & (Join-Path $PSScriptRoot "RegisterAndLaunchWtd.ps1") @registerArgs
}
