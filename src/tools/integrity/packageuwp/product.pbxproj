<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="ProductBuild" ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="$(CustomizationsRoot)\Mbs\common\Microsoft.Build.ModularBuild.ManifestProjectConfiguration.props" />
  <PropertyGroup>
    <EnableManifestImportInBuild>false</EnableManifestImportInBuild>
  </PropertyGroup>
  <ItemGroup>
    <WindowsManifest Include="sources.pkg.xml">
      <Type>LegacyTestPackage</Type>
    </WindowsManifest>
  </ItemGroup>
  <Import Project="$(CustomizationsRoot)\Mbs\common\Microsoft.Build.ModularBuild.ManifestProjectConfiguration.targets" />
</Project>