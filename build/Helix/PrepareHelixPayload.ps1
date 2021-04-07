[CmdLetBinding()]
Param(
    [string]$Platform,
    [string]$Configuration,
    [string]$ArtifactName='drop'
)

$payloadDir = "HelixPayload\$Configuration\$Platform"

$repoDirectory = Join-Path (Split-Path -Parent $script:MyInvocation.MyCommand.Path) "..\..\"
$nugetPackagesDir = Join-Path (Split-Path -Parent $script:MyInvocation.MyCommand.Path) "packages"
 
# Create the payload directory. Remove it if it already exists.
If(test-path $payloadDir)
{
    Remove-Item $payloadDir -Recurse
}
New-Item -ItemType Directory -Force -Path $payloadDir

# Copy files from nuget packages
Copy-Item "$nugetPackagesDir\microsoft.windows.apps.test.1.0.181203002\lib\netcoreapp2.1\*.dll" $payloadDir
Copy-Item "$nugetPackagesDir\Microsoft.Taef.10.58.210305002\build\Binaries\$Platform\*" $payloadDir
Copy-Item "$nugetPackagesDir\Microsoft.Taef.10.58.210305002\build\Binaries\$Platform\CoreClr\*" $payloadDir
New-Item -ItemType Directory -Force -Path "$payloadDir\.NETCoreApp2.1\"
Copy-Item "$nugetPackagesDir\runtime.win-$Platform.microsoft.netcore.app.2.1.0\runtimes\win-$Platform\lib\netcoreapp2.1\*" "$payloadDir\.NETCoreApp2.1\"
Copy-Item "$nugetPackagesDir\runtime.win-$Platform.microsoft.netcore.app.2.1.0\runtimes\win-$Platform\native\*" "$payloadDir\.NETCoreApp2.1\"

function Copy-If-Exists
{
    Param($source, $destinationDir)

    if (Test-Path $source)
    {
        Write-Host "Copy from '$source' to '$destinationDir'"
        Copy-Item -Force $source $destinationDir
    }
    else
    {
        Write-Host "'$source' does not exist."
    }
}

# Copy files from the 'drop' artifact dir
Copy-Item "$repoDirectory\Artifacts\$ArtifactName\$Configuration\$Platform\Test\*" $payloadDir -Recurse

# Copy files from the repo
New-Item -ItemType Directory -Force -Path "$payloadDir"
Copy-Item "build\helix\ConvertWttLogToXUnit.ps1" "$payloadDir"
Copy-Item "build\helix\OutputFailedTestQuery.ps1" "$payloadDir"
Copy-Item "build\helix\OutputSubResultsJsonFiles.ps1" "$payloadDir"
Copy-Item "build\helix\HelixTestHelpers.cs" "$payloadDir"
Copy-Item "build\helix\runtests.cmd" $payloadDir
Copy-Item "build\helix\InstallTestAppDependencies.ps1" "$payloadDir"
Copy-Item "build\Helix\EnsureMachineState.ps1" "$payloadDir"
