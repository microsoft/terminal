<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{2C2BEEF4-9333-4D05-B12A-1905CBF112F9}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>TerminalCoreUnitTests</RootNamespace>
    <ProjectName>UnitTests_TerminalCore</ProjectName>
    <TargetName>Terminal.Core.Unit.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>

  <Import Project="$(SolutionDir)\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(SolutionDir)\src\common.build.pre.props" />

  <ItemGroup>
    <ClCompile Include="ScreenSizeLimitsTest.cpp" />
    <ClCompile Include="SelectionTest.cpp" />
    <ClCompile Include="InputTest.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\buffer\out\lib\bufferout.vcxproj">
      <Project>{0cf235bd-2da0-407e-90ee-c467e8bbc714}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\base\lib\base.vcxproj">
      <Project>{af0a096a-8b3a-4949-81ef-7df8f0fee91f}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\terminal\input\lib\terminalinput.vcxproj">
      <Project>{1cf55140-ef6a-4736-a403-957e4f7430bb}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\terminal\parser\lib\parser.vcxproj">
      <Project>{3ae13314-1939-4dfa-9c14-38ca0834050c}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\types\lib\types.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820263}</Project>
    </ProjectReference>
    <ProjectReference Include="..\TerminalCore\lib\TerminalCore-lib.vcxproj">
      <Project>{ca5cad1a-abcd-429c-b551-8562ec954746}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="MockTermSettings.h" />
    <ClInclude Include="precomp.h" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;$(SolutionDir)src\inc;$(SolutionDir)src\inc\test;$(WinRT_IncludePath)\..\cppwinrt\winrt;"$(OpenConsoleDir)\src\cascadia\TerminalSettings\Generated Files";%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PrecompiledHeaderFile>precomp.h</PrecompiledHeaderFile>
    </ClCompile>
    <Link>
      <AdditionalDependencies>WindowsApp.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
