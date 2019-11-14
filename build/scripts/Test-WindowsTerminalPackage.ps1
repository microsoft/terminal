[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true, ValueFromPipeline=$true,
      HelpMessage="Path to the .appx/.msix to validate")]
    [string]
    $Path,

    [Parameter(HelpMessage="Path to Windows Kit")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $WindowsKitPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0"
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
    $RequiredInProcServers = ("TerminalApp.dll", "TerminalControl.dll", "TerminalConnection.dll")

    Write-Verbose "InProc Servers: $inProcServers"

    ForEach ($req in $RequiredInProcServers) {
        If ($req -NotIn $inProcServers) {
            Throw "Failed to find $req in InProcServer list $inProcServers"
        }
    }

    ### Check that we have an App.xbf (which is a proxy for our resources having been merged)
    $resourceXpath = '/PriInfo/ResourceMap/ResourceMapSubtree[@name="Files"]/NamedResource[@name="App.xbf"]'
    $AppXbf = $PRIFile.SelectSingleNode($resourceXpath)
    If ($null -eq $AppXbf) {
        Throw "Failed to find App.xbf (TerminalApp project) in resources.pri"
    }

    If (($null -eq (Get-Item "$AppxPackageRootPath\cpprest142_2_10.dll" -EA:Ignore)) -And
        ($null -eq (Get-Item "$AppxPackageRootPath\cpprest142_2_10d.dll" -EA:Ignore))) {
        Throw "Failed to find cpprest142_2_10.dll -- check the WAP packaging project"
    }

} Finally {
    Remove-Item -Recurse -Force $AppxPackageRootPath
}
