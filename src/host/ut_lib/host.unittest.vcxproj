<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{06EC74CB-9A12-429C-B551-8562EC954747}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>hostlib.unittest</RootNamespace>
    <ProjectName>Host.unittest</ProjectName>
    <TargetName>ConhostV2Lib.unittest</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <!-- DONT ADD NEW FILES HERE, ADD THEM TO host-common.vcxitems -->
  <Import Project="$(SolutionDir)src\host\host-common.vcxitems" />
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>INLINE_TEST_METHOD_MARKUP;UNIT_TESTING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
