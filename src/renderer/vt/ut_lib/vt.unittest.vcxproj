<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{990F2657-8580-4828-943F-5DD657D11843}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>vt.unittest</RootNamespace>
    <ProjectName>RendererVt.unittest</ProjectName>
    <TargetName>ConRenderVt.unittest</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <Import Project="$(SolutionDir)src\renderer\vt\vt-renderer-common.vcxitems" />
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>INLINE_TEST_METHOD_MARKUP;UNIT_TESTING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- DONT ADD NEW FILES HERE, ADD THEM TO vt-renderer-common.vcxitems -->
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <!-- <Import Project="$(SolutionDir)src\common.build.tests.props" /> -->
</Project>
