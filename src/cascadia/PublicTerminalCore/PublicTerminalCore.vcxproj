<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <VCProjectVersion>16.0</VCProjectVersion>
    <ProjectGuid>{84848BFA-931D-42CE-9ADF-01EE54DE7890}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>PublicTerminalCore</RootNamespace>
    <ProjectName>PublicTerminalCore</ProjectName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>

  <Import Project="..\..\..\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(SolutionDir)src\common.build.pre.props" />

  <ItemGroup>
    <ClCompile Include="pch.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="HwndTerminal.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="HwndTerminal.hpp" />
    <ClInclude Include="pch.h" />
  </ItemGroup>

  <ItemGroup>
    <ProjectReference Include="..\..\terminal\input\lib\terminalinput.vcxproj">
      <Project>{1cf55140-ef6a-4736-a403-957e4f7430bb}</Project>
    </ProjectReference>
    <ProjectReference Include="..\TerminalCore\lib\TerminalCore-lib.vcxproj">
      <Project>{ca5cad1a-abcd-429c-b551-8562ec954746}</Project>
    </ProjectReference>
    <ProjectReference Include="$(OpenConsoleDir)src\types\lib\types.vcxproj">
      <Project>{18D09A24-8240-42D6-8CB6-236EEE820263}</Project>
    </ProjectReference>
    <ProjectReference Include="$(OpenConsoleDir)src\buffer\out\lib\bufferout.vcxproj">
      <Project>{0cf235bd-2da0-407e-90ee-c467e8bbc714}</Project>
    </ProjectReference>
    <ProjectReference Include="$(OpenConsoleDir)src\renderer\base\lib\base.vcxproj">
      <Project>{af0a096a-8b3a-4949-81ef-7df8f0fee91f}</Project>
    </ProjectReference>
    <ProjectReference Include="$(OpenConsoleDir)src\terminal\parser\lib\parser.vcxproj">
      <Project>{3ae13314-1939-4dfa-9c14-38ca0834050c}</Project>
    </ProjectReference>
    <ProjectReference Include="$(OpenConsoleDir)src\renderer\dx\lib\dx.vcxproj">
      <Project>{48d21369-3d7b-4431-9967-24e81292cf62}</Project>
    </ProjectReference>
  </ItemGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeaderFile>pch.h</PrecompiledHeaderFile>
    </ClCompile>
  </ItemDefinitionGroup>

  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
