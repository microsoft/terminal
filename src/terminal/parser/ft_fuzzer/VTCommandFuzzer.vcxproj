<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{96927B31-D6E8-4ABD-B03E-A5088A30BEBE}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>VTCommandFuzzer</RootNamespace>
    <ProjectName>TerminalParser.Fuzzer</ProjectName>
    <TargetName>VTCommandFuzzer</TargetName>
    <ConfigurationType>Application</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="stdafx.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="VTCommandFuzzer.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="fuzzing_directed.h" />
    <ClInclude Include="fuzzing_logic.h" />
    <ClInclude Include="memallocator.h" />
    <ClInclude Include="stdafx.h" />
    <ClInclude Include="string_helper.h" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeaderFile>stdafx.h</PrecompiledHeaderFile>
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
