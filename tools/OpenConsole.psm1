
# The project's root directory.
Set-Item -force -path "env:OpenConsoleRoot" -value "$PSScriptRoot\.."

#.SYNOPSIS
# Finds and imports a module that should be local to the project
#.PARAMETER ModuleName
# The name of the module to import
function Import-LocalModule
{
    [CmdletBinding()]
    param(
        [parameter(Mandatory=$true, Position=0)]
        [string]$Name
    )

    $ErrorActionPreference = 'Stop'

    $modules_root = "$env:OpenConsoleRoot\.PowershellModules"

    $local = $null -eq (Get-Module -Name $Name)

    if (-not $local)
    {
        return
    }

    if (-not (Test-Path $modules_root)) {
        New-Item $modules_root -ItemType 'directory' | Out-Null
    }

    if (-not (Test-Path "$modules_root\$Name")) {
        Write-Verbose "$Name not downloaded -- downloading now"
        $module = Find-Module "$Name"
        $version = $module.Version

        Write-Verbose "Saving $Name to $modules_root"
        Save-Module -InputObject $module -Path $modules_root
        Import-Module "$modules_root\$Name\$version\$Name.psd1"
    } else {
        Write-Verbose "$Name already downloaded"
        $versions = Get-ChildItem "$modules_root\$Name" | Sort-Object

        Get-ChildItem -Path "$modules_root\$Name\$($versions[0])\$Name.psd1" | Import-Module
    }
}

#.SYNOPSIS
# Grabs all environment variable set after vcvarsall.bat is called and pulls
# them into the Powershell environment.
function Set-MsbuildDevEnvironment
{
    [CmdletBinding()]
    param(
        [switch]$Prerelease
    )

    $ErrorActionPreference = 'Stop'

    Import-LocalModule -Name 'VSSetup'

    Write-Verbose 'Searching for VC++ instances'
    $vsinfo = `
        Get-VSSetupInstance  -All -Prerelease:$Prerelease `
        | Select-VSSetupInstance `
            -Latest -Product * `
            -Require 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'

    $vspath = $vsinfo.InstallationPath

    switch ($env:PROCESSOR_ARCHITECTURE) {
        "amd64" { $arch = "x64" }
        "x86" { $arch = "x86" }
        default { throw "Unknown architecture: $switch" }
    }

    $vcvarsall = "$vspath\VC\Auxiliary\Build\vcvarsall.bat"

    Write-Verbose 'Setting up environment variables'
    cmd /c ("`"$vcvarsall`" $arch & set") | ForEach-Object {
        if ($_ -match '=')
        {
            $s = $_.Split("=");
            Set-Item -force -path "env:\$($s[0])" -value "$($s[1])"
        }
    }

    Write-Host "Dev environment variables set" -ForegroundColor Green
}

#.SYNOPSIS
# Runs a Taef test suite in a new OpenConsole window.
#
#.PARAMETER OpenConsolePath
# Path to the OpenConsole.exe to run.
#
#.PARAMETER $TaefPath
# Path to the taef.exe to run.
#
#.PARAMETER $TestDll
# Path to the test DLL to run with Taef.
#
#.PARAMETER $TaefArgs
# Any arguments to path to Taef.
function Invoke-TaefInNewWindow()
{
    [CmdletBinding()]
    Param (
        [parameter(Mandatory=$true)]
        [string]$OpenConsolePath,

        [parameter(Mandatory=$true)]
        [string]$TaefPath,

        [parameter(Mandatory=$true)]
        [string]$TestDll,

        [parameter(Mandatory=$false)]
        [string]$TaefArgs
    )

    Start-Process $OpenConsolePath -Wait -ArgumentList "powershell.exe $TaefPath $TestDll $TaefArgs; Read-Host 'Press enter to continue...'"
}

#.SYNOPSIS
# Runs OpenConsole's tests. Will only run unit tests by default. Each ft test is
# run in its own window. Note that the uia tests will move the mouse around, so
# it must be left alone for the duration of the test.
#
#.PARAMETER AllTests
# When set, all tests will be run.
#
#.PARAMETER FTOnly
# When set, only ft tests will be run.
#
#.PARAMETER Test
# Can be used to specify that only a particular test should be run.
# Current values allowed are: host, interactivityWin32, terminal, adapter,
# feature, uia, textbuffer.
#
#.PARAMETER TaefArgs
# Used to pass any additional arguments to the test runner.
#
#.PARAMETER Platform
# The platform of the OpenConsole tests to run. Can be "x64" or "x86".
# Defaults to "x64".
#
#.PARAMETER Configuration
# The configuration of the OpenConsole tests to run. Can be "Debug" or
# "Release". Defaults to "Debug".
function Invoke-OpenConsoleTests()
{
    [CmdletBinding()]
    Param (
        [parameter(Mandatory=$false)]
        [switch]$AllTests,

        [parameter(Mandatory=$false)]
        [switch]$FTOnly,

        [parameter(Mandatory=$false)]
        [ValidateSet('host', 'interactivityWin32', 'terminal', 'adapter', 'feature', 'uia', 'textbuffer', 'types', 'terminalCore', 'terminalApp', 'localTerminalApp')]
        [string]$Test,

        [parameter(Mandatory=$false)]
        [string]$TaefArgs,

        [parameter(Mandatory=$false)]
        [ValidateSet('x64', 'x86')]
        [string]$Platform = "x64",

        [parameter(Mandatory=$false)]
        [ValidateSet('Debug', 'Release')]
        [string]$Configuration = "Debug"

    )

    if (($AllTests -and $FTOnly) -or ($AllTests -and $Test) -or ($FTOnly -and $Test))
    {
        Write-Host "Invalid combination of flags" -ForegroundColor Red
        return
    }
    $OpenConsolePlatform = $Platform
    if ($Platform -eq 'x86')
    {
        $OpenConsolePlatform = 'Win32'
    }
    $OpenConsolePath = "$env:OpenConsoleroot\bin\$OpenConsolePlatform\$Configuration\OpenConsole.exe"
    $RunTePath = "$env:OpenConsoleRoot\tools\runte.cmd"
    $TaefExePath = "$env:OpenConsoleRoot\packages\Taef.Redist.Wlk.10.38.190610001-uapadmin\build\Binaries\$Platform\te.exe"
    $BinDir = "$env:OpenConsoleRoot\bin\$OpenConsolePlatform\$Configuration"
    [xml]$TestConfig = Get-Content "$env:OpenConsoleRoot\tools\tests.xml"

    # check if WinAppDriver needs to be started
    $WinAppDriverExe = $null
    if ($AllTests -or $FtOnly -or $Test -eq "uia")
    {
        $WinAppDriverExe = [Diagnostics.Process]::Start("$env:OpenConsoleRoot\dep\WinAppDriver\WinAppDriver.exe")
    }

    # select tests to run
    if ($AllTests)
    {
        $TestsToRun = $TestConfig.tests.test
    }
    elseif ($FTOnly)
    {
        $TestsToRun = $TestConfig.tests.test | Where-Object { $_.type -eq "ft" }
    }
    elseif ($Test)
    {
        $TestsToRun = $TestConfig.tests.test | Where-Object { $_.name -eq $Test }
    }
    else
    {
        # run unit tests by default
        $TestsToRun = $TestConfig.tests.test | Where-Object { $_.type -eq "unit" }
    }

    # run selected tests
    foreach ($t in $TestsToRun)
    {
        if ($t.type -eq "unit")
        {
            & $TaefExePath "$BinDir\$($t.binary)" $TaefArgs
        }
        elseif ($t.type -eq "ft")
        {
            Invoke-TaefInNewWindow -OpenConsolePath $OpenConsolePath -TaefPath $TaefExePath -TestDll "$BinDir\$($t.binary)" -TaefArgs $TaefArgs
        }
        else
        {
            Write-Host "Invalid test type $t.type for test: $t.name" -ForegroundColor Red
            return
        }
    }

    # stop running WinAppDriver if it was launched
    if ($WinAppDriverExe)
    {
        Stop-Process -Id $WinAppDriverExe.Id
    }
}


#.SYNOPSIS
# Builds OpenConsole.sln using msbuild. Any arguments get passed on to msbuild.
function Invoke-OpenConsoleBuild()
{
    & "$env:OpenConsoleRoot\dep\nuget\nuget.exe" restore "$env:OpenConsoleRoot\OpenConsole.sln"
    msbuild.exe "$env:OpenConsoleRoot\OpenConsole.sln" @args
}

#.SYNOPSIS
# Launches an OpenConsole process.
#
#.PARAMETER Platform
# The platform of the OpenConsole executable to launch. Can be "x64" or "x86".
# Defaults to "x64".
#
#.PARAMETER Configuration
# The configuration of the OpenConsole executable to launch. Can be "Debug" or
# "Release". Defaults to "Debug".
function Start-OpenConsole()
{
    [CmdletBinding()]
    Param (
        [parameter(Mandatory=$false)]
        [string]$Platform = "x64",

        [parameter(Mandatory=$false)]
        [string]$Configuration = "Debug"
    )
    if ($Platform -like "x86")
    {
        $Platform = "Win32"
    }
    & "$env:OpenConsoleRoot\bin\$Platform\$Configuration\OpenConsole.exe"
}

#.SYNOPSIS
# Launches an OpenConsole process and attaches the default debugger.
#
#.PARAMETER Platform
# The platform of the OpenConsole executable to launch. Can be "x64" or "x86".
# Defaults to "x64".
#
#.PARAMETER Configuration
# The configuration of the OpenConsole executable to launch. Can be "Debug" or
# "Release". Defaults to "Debug".
function Debug-OpenConsole()
{
    [CmdletBinding()]
    Param (
        [parameter(Mandatory=$false)]
        [string]$Platform = "x64",

        [parameter(Mandatory=$false)]
        [string]$Configuration = "Debug"
    )
    if ($Platform -like "x86")
    {
        $Platform = "Win32"
    }
    $process = [Diagnostics.Process]::Start("$env:OpenConsoleRoot\bin\$Platform\$Configuration\OpenConsole.exe")
    Debug-Process -Id $process.Id
}

#.SYNOPSIS
# runs clang-format on list of files
#
#.PARAMETER Path
# The full paths to the files to format
function Invoke-ClangFormat {
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory=$true,ValueFromPipeline=$true)]
        [string[]]$Path
    )

    Process {
        ForEach($_ in $Path) {
            Try {
                $n = Get-Item $_ -ErrorAction Stop | Select -Expand FullName
                & "$env:OpenconsoleRoot/dep/llvm/clang-format" -i $n
            } Catch {
                Write-Error $_
            }
        }
    }
}

#.SYNOPSIS
# runs code formatting on all c++ files
function Invoke-CodeFormat() {
    Get-ChildItem -Recurse "$env:OpenConsoleRoot/src" -Include *.cpp, *.hpp, *.h |
      Where FullName -NotLike "*Generated Files*" |
      Invoke-ClangFormat
}

Export-ModuleMember -Function Set-MsbuildDevEnvironment,Invoke-OpenConsoleTests,Invoke-OpenConsoleBuild,Start-OpenConsole,Debug-OpenConsole,Invoke-CodeFormat
