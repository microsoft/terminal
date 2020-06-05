<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{48D21369-3D7B-4431-9967-24E81292CF63}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>uia</RootNamespace>
    <ProjectName>RendererUia</ProjectName>
    <TargetName>ConRenderUia</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\UiaRenderer.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\UiaRenderer.hpp" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
