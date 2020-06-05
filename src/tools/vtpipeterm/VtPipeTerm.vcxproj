<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{814DBDDE-894E-4327-A6E1-740504850098}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>VtPipeTerm</RootNamespace>
    <ProjectName>VtPipeTerm</ProjectName>
    <TargetName>VtPipeTerm</TargetName>
    <ConfigurationType>Application</ConfigurationType>
    <WindowsTargetPlatformVersion>10.0.18362.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="..\..\common.build.pre.props" />
  <ItemGroup>
    <ClInclude Include="VtConsole.hpp" />
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="main.cpp" />
    <ClCompile Include="VtConsole.cpp" />
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
