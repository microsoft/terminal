
param (
    [parameter(Mandatory=$true, Position=0)]
    [string]$SxSManifest,

    [parameter(Mandatory=$true, Position=1)]
    [string]$AppxManifestPrototype,

    [parameter(Mandatory=$true, Position=2)]
    [string]$OutPath
)

$manifestPath = $manifest.FullName

[xml]$manifestData = Get-Content $SxSManifest


$assembly = $manifestData.assembly
$files = $assembly.file
$classes = $files.activatableClass


[xml]$appxPrototypeData = Get-Content $AppxManifestPrototype

# You need to make sure each element we add is part of the same namespace as the
# Package, otherwise powershell will append a bunch of `xmlns=""` properties
# that will make the appx deployment reject the manifest.
$rootNS = $appxPrototypeData.Package.NamespaceURI
$Extensions = $appxPrototypeData.CreateNode("element", "Extensions", $rootNS)


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

$appxPrototypeData.Package.AppendChild($Extensions) | Out-Null

$appxPrototypeData.save($OutPath)


# Left as a helper for debugging:
# $StringWriter = New-Object System.IO.StringWriter;
# $XmlWriter = New-Object System.Xml.XmlTextWriter $StringWriter;
# $XmlWriter.Formatting = "indented";
# $appxPrototypeData.WriteTo($XmlWriter);
# $XmlWriter.Flush();
# $StringWriter.Flush();
# Write-Output $StringWriter.ToString();

