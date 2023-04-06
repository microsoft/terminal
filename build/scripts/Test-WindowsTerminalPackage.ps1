[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true, ValueFromPipeline=$true,
      HelpMessage="Path to the .appx/.msix to validate")]
    [string]
    $Path,

    [Parameter(HelpMessage="Path to Windows Kit")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $WindowsKitPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0"
)

$ErrorActionPreference = "Stop"

If ($null -Eq (Get-Item $WindowsKitPath -EA:SilentlyContinue)) {
    Write-Error "Could not find a windows SDK at at `"$WindowsKitPath`".`nMake sure that WindowsKitPath points to a valid SDK."
    Exit 1
}

$makeAppx = "$WindowsKitPath\x86\MakeAppx.exe"
$makePri = "$WindowsKitPath\x86\MakePri.exe"

Function Expand-ApplicationPackage {
    Param(
        [Parameter(Mandatory, ValueFromPipeline)]
        [string]
        $Path
    )

    $sentinelFile = New-TemporaryFile
    $directory = New-Item -Type Directory "$($sentinelFile.FullName)_Package"
    Remove-Item $sentinelFile -Force -EA:Ignore

    & $makeAppx unpack /p $Path /d $directory /nv /o

    If ($LastExitCode -Ne 0) {
        Throw "Failed to expand AppX"
    }

    $directory
}

Write-Verbose "Expanding $Path"
$AppxPackageRoot = Expand-ApplicationPackage $Path
$AppxPackageRootPath = $AppxPackageRoot.FullName

Write-Verbose "Expanded to $AppxPackageRootPath"

Try {
    & $makePri dump /if "$AppxPackageRootPath\resources.pri" /of "$AppxPackageRootPath\resources.pri.xml" /o
    If ($LastExitCode -Ne 0) {
        Throw "Failed to dump PRI"
    }

    $Manifest = [xml](Get-Content "$AppxPackageRootPath\AppxManifest.xml")
    $PRIFile = [xml](Get-Content "$AppxPackageRootPath\resources.pri.xml")

    ### Check the activatable class entries for a few DLLs we need.
    $inProcServers = $Manifest.Package.Extensions.Extension.InProcessServer.Path
    $RequiredInProcServers = ("TerminalApp.dll", "Microsoft.Terminal.Control.dll", "Microsoft.Terminal.Remoting.dll", "Microsoft.Terminal.Settings.Editor.dll", "Microsoft.Terminal.Settings.Model.dll", "TerminalConnection.dll")

    Write-Verbose "InProc Servers: $inProcServers"

    ForEach ($req in $RequiredInProcServers) {
        If ($req -NotIn $inProcServers) {
            Throw "Failed to find $req in InProcServer list $inProcServers"
        }
    }

    $dependencies = $Manifest.Package.Dependencies.PackageDependency.Name
    $depsHasVclibsDesktop = ("Microsoft.VCLibs.140.00.UWPDesktop" -in $dependencies) -or ("Microsoft.VCLibs.140.00.Debug.UWPDesktop" -in $dependencies)
    $depsHasVclibsAppX = ("Microsoft.VCLibs.140.00" -in $dependencies) -or ("Microsoft.VCLibs.140.00.Debug" -in $dependencies)
    $filesHasVclibsDesktop = ($null -ne (Get-Item "$AppxPackageRootPath\vcruntime140.dll" -EA:Ignore)) -or ($null -ne (Get-Item "$AppxPackageRootPath\vcruntime140d.dll" -EA:Ignore))
    $filesHasVclibsAppX = ($null -ne (Get-Item "$AppxPackageRootPath\vcruntime140_app.dll" -EA:Ignore)) -or ($null -ne (Get-Item "$AppxPackageRootPath\vcruntime140d_app.dll" -EA:Ignore))

    If ($filesHasVclibsDesktop) {
        Throw "Package contains the desktop VCLibs"
    }

    If ($depsHasVclibsDesktop) {
        Throw "Package has a dependency on the desktop VCLibs"
    }

    If ($filesHasVclibsAppX) {
        Throw "Package contains the AppX VCLibs"
    }

    If ($depsHasVclibsAppX) {
        Throw "Package has a dependency on the AppX VCLibs"
    }

    ### Check that we have an App.xbf (which is a proxy for our resources having been merged)
    $resourceXpath = '/PriInfo/ResourceMap/ResourceMapSubtree[@name="Files"]/NamedResource[@name="App.xbf"]'
    $AppXbf = $PRIFile.SelectSingleNode($resourceXpath)
    If ($null -eq $AppXbf) {
        Throw "Failed to find App.xbf (TerminalApp project) in resources.pri"
    }

    If (($null -eq (Get-Item "$AppxPackageRootPath\wtd.exe" -EA:Ignore)) -And
        ($null -eq (Get-Item "$AppxPackageRootPath\wt.exe" -EA:Ignore))) {
        Throw "Failed to find wt.exe/wtd.exe -- check the WAP packaging project"
    }

    If ($null -eq (Get-Item "$AppxPackageRootPath\OpenConsole.exe" -EA:Ignore)) {
        Throw "Failed to find OpenConsole.exe -- check the WAP packaging project"
    }

} Finally {
    Remove-Item -Recurse -Force $AppxPackageRootPath
}
