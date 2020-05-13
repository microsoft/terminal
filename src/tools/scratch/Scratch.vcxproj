<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{ED82003F-FC5D-4E94-8B36-F480018ED064}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>Scratch</RootNamespace>
    <ProjectName>Scratch</ProjectName>
    <TargetName>Scratch</TargetName>
    <ConfigurationType>Application</ConfigurationType>
  </PropertyGroup>
  <Import Project="..\..\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="main.cpp" />
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
