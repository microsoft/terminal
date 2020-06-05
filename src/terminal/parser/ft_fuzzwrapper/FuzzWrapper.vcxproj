<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{F210A4AE-E02A-4BFC-80BB-F50A672FE763}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>FuzzWrapper</RootNamespace>
    <ProjectName>TerminalParser.FuzzWrapper</ProjectName>
    <TargetName>ConTerm.Parser.FuzzWrapper</TargetName>
    <ConfigurationType>Application</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="main.cpp" />
    <ClCompile Include="echoDispatch.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="echoDispatch.hpp" />
    <ClInclude Include="precomp.h" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\lib\parser.vcxproj">
      <Project>{3ae13314-1939-4dfa-9c14-38ca0834050c}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
