<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{1C959542-BAC2-4E55-9A6D-13251914CBB9}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>gdi</RootNamespace>
    <ProjectName>RendererGdi</ProjectName>
    <TargetName>ConRenderGdi</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="..\invalidate.cpp" />
    <ClCompile Include="..\math.cpp" />
    <ClCompile Include="..\paint.cpp" />
    <ClCompile Include="..\state.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\gdirenderer.hpp" />
    <ClInclude Include="..\precomp.h" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
