<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{345FD5A4-B32B-4F29-BD1C-B033BD2C35CC}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>propslib</RootNamespace>
    <ProjectName>PropertiesLibrary</ProjectName>
    <TargetName>ConProps</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="RegistrySerialization.cpp" />
    <ClCompile Include="ShortcutSerialization.cpp" />
    <ClCompile Include="TrueTypeFontList.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="conpropsp.hpp" />
    <ClInclude Include="RegistrySerialization.hpp" />
    <ClInclude Include="ShortcutSerialization.hpp" />
    <ClInclude Include="TrueTypeFontList.hpp" />
    <ClInclude Include="precomp.h" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
