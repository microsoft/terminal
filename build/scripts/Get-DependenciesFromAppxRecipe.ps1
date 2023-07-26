[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true, ValueFromPipeline=$true,
      HelpMessage="Path to the .appxrecipe to parse")]
    [string]
    $Path
)

$Recipe = [xml](Get-Content $Path)
$xm = [System.Xml.XmlNamespaceManager]::new($Recipe.NameTable)
$xm.AddNamespace("x", $Recipe.Project.xmlns)
$recipePackageXpath = '/x:Project/x:ItemGroup/x:ResolvedSDKReference[./x:Architecture[text()="{0}"]]' -f $Recipe.Project.PropertyGroup.PackageArchitecture
$DependencyNodes = $Recipe.SelectNodes($recipePackageXpath, $xm)

$DependencyNodes | % {
	$l = [Uri]::UnescapeDataString($_.AppxLocation)
	Get-Item $l
}
