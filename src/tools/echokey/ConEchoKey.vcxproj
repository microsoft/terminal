<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{814CBEEE-894E-4327-A6E1-740504850098}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>ConEchoKey</RootNamespace>
    <ProjectName>ConEchoKey</ProjectName>
    <TargetName>ConEchoKey</TargetName>
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
