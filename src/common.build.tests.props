<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>UNIT_TESTING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  <Import Project="$(MSBuildThisFileDirectory)..\packages\Taef.Redist.Wlk.10.38.190610001-uapadmin\build\Taef.Redist.Wlk.targets" Condition="Exists('$(MSBuildThisFileDirectory)..\packages\Taef.Redist.Wlk.10.38.190610001-uapadmin\build\Taef.Redist.Wlk.targets')" />
  <Target Name="EnsureNuGetPackageBuildImports" BeforeTargets="PrepareForBuild">
    <PropertyGroup>
      <ErrorText>This project references NuGet package(s) that are missing on this computer. Use NuGet Package Restore to download them.  For more information, see http://go.microsoft.com/fwlink/?LinkID=322105. The missing file is {0}.</ErrorText>
    </PropertyGroup>
    <Error Condition="!Exists('$(MSBuildThisFileDirectory)..\packages\Taef.Redist.Wlk.10.38.190610001-uapadmin\build\Taef.Redist.Wlk.targets')" Text="$([System.String]::Format('$(ErrorText)', '$(MSBuildThisFileDirectory)..\packages\Taef.Redist.Wlk.10.38.190610001-uapadmin\build\Taef.Redist.Wlk.targets'))" />
  </Target>
</Project>
