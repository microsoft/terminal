<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{3AE13314-1939-4DFA-9C14-38CA0834050C}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>parser</RootNamespace>
    <ProjectName>TerminalParser</ProjectName>
    <TargetName>ConTermParser</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props"/>
  <!-- ONLY ADD CLOSED-SOURCE FILES HERE, OPEN-SOURCE FILES GO IN parser-common.vcxitems -->
  <Import Project="$(SolutionDir)src\terminal\parser\parser-common.vcxitems" />
  <ItemGroup>
    <ClCompile Include="..\InputStateMachineEngine.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\InputStateMachineEngine.hpp" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props"/>
</Project>
