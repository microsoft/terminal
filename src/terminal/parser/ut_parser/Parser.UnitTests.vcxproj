<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{12144E07-FE63-4D33-9231-748B8D8C3792}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>ParserUnitTests</RootNamespace>
    <ProjectName>TerminalParser.UnitTests</ProjectName>
    <TargetName>ConParser.Unit.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />


  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
  </ItemGroup>

  <!-- Only add closed-source files, dependencies to this file.
      Any open-source files can go in Parser.UnitTests-common.vcxproj -->
  <ItemGroup>
    <ClCompile Include="InputEngineTest.cpp" />
    <ClCompile Include="OutputEngineTest.cpp" />
    <ClCompile Include="StateMachineTest.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>

  <ItemGroup>
    <ProjectReference Include="..\..\..\types\lib\types.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820263}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\adapter\lib\adapter.vcxproj">
      <Project>{dcf55140-ef6a-4736-a403-957e4f7430bb}</Project>
    </ProjectReference>
    <ProjectReference Include="..\lib\parser.vcxproj">
      <Project>{3ae13314-1939-4dfa-9c14-38ca0834050c}</Project>
    </ProjectReference>
  </ItemGroup>


  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
