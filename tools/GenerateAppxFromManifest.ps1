# This script is used for taking all the activatable classes from a SxS manifest
# and adding them as Extensions to an Appxmanifest.xml.
# Params:
#  - SxSManifest: The path to the SxS manifest to get the types from
#  - AppxManifestPrototype: The path to an AppxManifest.xml-style XML document to add the Extensions to
#  - SxSManifest: The path to write the updated XML doc to.

param (
    [parameter(Mandatory=$true, Position=0)]
    [string]$SxSManifest,

    [parameter(Mandatory=$true, Position=1)]
    [string]$AppxManifestPrototype,

    [parameter(Mandatory=$true, Position=2)]
    [string]$OutPath
)

# Load the xml files.
[xml]$manifestData = Get-Content $SxSManifest
[xml]$appxPrototypeData = Get-Content $AppxManifestPrototype

# You need to make sure each element we add is part of the same namespace as the
# Package, otherwise powershell will append a bunch of `xmlns=""` properties
# that will make the appx deployment reject the manifest.
$rootNS = $appxPrototypeData.Package.NamespaceURI

# Create an XML element for all the extensions we're adding.
$Extensions = $appxPrototypeData.CreateNode("element", "Extensions", $rootNS)

$assembly = $manifestData.assembly
$files = $assembly.file
$files | ForEach-Object {

    $Extension = $appxPrototypeData.CreateNode("element", "Extension", $rootNS)
    $Extension.SetAttribute("Category", "windows.activatableClass.inProcessServer")

    $InProcessServer = $appxPrototypeData.CreateNode("element", "InProcessServer", $rootNS)
    $Path = $appxPrototypeData.CreateNode("element", "Path", $rootNS)

    # You need to stash the result here, otherwise a blank line will be echod to
    # the console.
    $placeholder = $Path.InnerText = $_.name

    $InProcessServer.AppendChild($Path)
    $Extension.AppendChild($InProcessServer) | Out-Null

    foreach($class in $_.activatableClass) {
        $ActivatableClass = $appxPrototypeData.CreateNode("element", "ActivatableClass", $rootNS)
        $ActivatableClass.SetAttribute("ActivatableClassId", $class.name)
        $ActivatableClass.SetAttribute("ThreadingModel", $class.threadingModel)

        $InProcessServer.AppendChild($ActivatableClass) | Out-Null
    }

    $Extensions.AppendChild($Extension) | Out-Null

}

# Add our fully constructed list of extensions to the original Appxmanifest prototype
$appxPrototypeData.Package.AppendChild($Extensions) | Out-Null

# Write the modified xml back out.
$appxPrototypeData.save($OutPath)

# Left as a helper for debugging:
# $StringWriter = New-Object System.IO.StringWriter;
# $XmlWriter = New-Object System.Xml.XmlTextWriter $StringWriter;
# $XmlWriter.Formatting = "indented";
# $appxPrototypeData.WriteTo($XmlWriter);
# $XmlWriter.Flush();
# $StringWriter.Flush();
# Write-Output $StringWriter.ToString();

