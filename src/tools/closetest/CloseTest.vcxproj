<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{C7A6A5D9-60BE-4AEB-A5F6-AFE352F86CBB}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>CloseTest</RootNamespace>
    <ProjectName>CloseTest</ProjectName>
    <TargetName>CloseTest</TargetName>
    <ConfigurationType>Application</ConfigurationType>
  </PropertyGroup>
  <Import Project="..\..\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="closetest.cpp" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="..\..\common.build.post.props" />
  <Import Project="..\..\common.build.tests.props" />
</Project>
