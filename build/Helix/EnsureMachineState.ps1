$scriptDirectory = $script:MyInvocation.MyCommand.Path | Split-Path -Parent

# List all processes to aid debugging:
Write-Host "All processes running:"
Get-Process

tasklist /svc

# Add this test directory as an exclusion for Windows Defender
Write-Host "Add $scriptDirectory as Exclusion Path"
Add-MpPreference -ExclusionPath $scriptDirectory
Write-Host "Add $($env:HELIX_CORRELATION_PAYLOAD) as Exclusion Path"
Add-MpPreference -ExclusionPath $env:HELIX_CORRELATION_PAYLOAD
Get-MpPreference
Get-MpComputerStatus


# Minimize all windows:
$shell = New-Object -ComObject "Shell.Application"
$shell.minimizeall()

# Kill any instances of Windows Security Alert:
$windowTitleToMatch = "*Windows Security Alert*"
$procs = Get-Process | Where {$_.MainWindowTitle -like "*Windows Security Alert*"}
foreach ($proc in $procs)
{
    Write-Host "Found process with '$windowTitleToMatch' title: $proc"
    $proc.Kill();
}

# Kill processes by name that are known to interfere with our tests:
$processNamesToStop = @("Microsoft.Photos", "WinStore.App", "SkypeApp", "SkypeBackgroundHost", "OneDriveSetup", "OneDrive")
foreach($procName in $processNamesToStop)
{
    Write-Host "Attempting to kill $procName if it is running"
    Stop-Process -ProcessName $procName -Verbose -ErrorAction Ignore   
}
Write-Host "All processes running after attempting to kill unwanted processes:"
Get-Process

tasklist /svc

$platform = $env:testbuildplatform
if(!$platform)
{
    $platform = "x86"
}

function UninstallApps {
    Param([string[]]$appsToUninstall)

    foreach($pkgName in $appsToUninstall)
    {
        foreach($pkg in (Get-AppxPackage $pkgName).PackageFullName) 
        {
            Write-Output "Removing: $pkg" 
            Remove-AppxPackage $pkg
        } 
    }
}

function UninstallTestApps {
    Param([string[]]$appsToUninstall)

    foreach($pkgName in $appsToUninstall)
    {
        foreach($pkg in (Get-AppxPackage $pkgName).PackageFullName) 
        {
            Write-Output "Removing: $pkg" 
            Remove-AppxPackage $pkg
        }

        # Sometimes an app can get into a state where it is no longer returned by Get-AppxPackage, but it is still present
        # which prevents other versions of the app from being installed.
        # To handle this, we can directly call Remove-AppxPackage against the full name of the package. However, without
        # Get-AppxPackage to find the PackageFullName, we just have to manually construct the name.
        $packageFullName = "$($pkgName)_1.0.0.0_$($platform)__8wekyb3d8bbwe" 
        Write-Host "Removing $packageFullName if installed"
        Remove-AppPackage $packageFullName -ErrorVariable appxerror -ErrorAction SilentlyContinue 
        if($appxerror)
        {
            foreach($error in $appxerror)
            {
                # In most cases, Remove-AppPackage will fail due to the package not being found. Don't treat this as an error.
                if(!($error.Exception.Message -match "0x80073CF1"))
                {
                    Write-Error $error
                }
            }
        }
        else
        {
            Write-Host "Sucessfully removed $packageFullName"
        }
    }
}

Write-Host "Uninstall AppX packages that are known to cause issues with our tests"
UninstallApps("*Skype*", "*Windows.Photos*")

Write-Host "Uninstall any of our test apps that may have been left over from previous test runs"
UninstallTestApps("NugetPackageTestApp", "NugetPackageTestAppCX", "IXMPTestApp", "MUXControlsTestApp")

Write-Host "Uninstall MUX Framework package that may have been left over from previous test runs"
# We don't want to uninstall all versions of the MUX Framework package, as there may be other apps preinstalled on the system 
# that depend on it. We only uninstall the Framework package that corresponds to the version of MUX that we are testing.
[xml]$versionData = (Get-Content "version.props")
$versionMajor = $versionData.GetElementsByTagName("MUXVersionMajor").'#text'
$versionMinor = $versionData.GetElementsByTagName("MUXVersionMinor").'#text'
UninstallApps("Microsoft.UI.Xaml.$versionMajor.$versionMinor")

Get-Process