[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true, ValueFromPipeline=$true,
      HelpMessage="Path to the .appxrecipe to parse")]
    [string]
    $Path
)

$Recipe = [xml](Get-Content $Path)
$ResolvedSDKReferences = $Recipe.Project.ItemGroup.ResolvedSDKReference

$ResolvedSDKReferences |
    Where-Object Architecture -eq $Recipe.Project.PropertyGroup.PackageArchitecture |
    ForEach-Object {
        $l = [Uri]::UnescapeDataString($_.AppxLocation)
        Get-Item $l
    }
