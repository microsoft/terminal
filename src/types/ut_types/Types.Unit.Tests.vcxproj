<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{34de34d3-1cd6-4ee3-8bd9-a26b5b27ec73}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>TypesUnitTests</RootNamespace>
    <ProjectName>Types.Unit.Tests</ProjectName>
    <TargetName>Types.Unit.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="UTF8OutPipeReaderTests.cpp" />
    <ClCompile Include="UtilsTests.cpp" />
    <ClCompile Include="UuidTests.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\lib\types.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820263}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;$(SolutionDir)src\inc;$(SolutionDir)src\inc\test;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
