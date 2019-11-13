<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{06EC74CB-9A12-428C-B551-8537EC964726}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>OneCore</RootNamespace>
    <ProjectName>InteractivityOneCore</ProjectName>
    <TargetName>ConInteractivityOneCoreLib</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>IS_INTERACTIVITYBASE_CONSUMER;_WINDLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="..\..\..\host\newdelete.cpp" />
    <ClCompile Include="..\AccessibilityNotifier.cpp" />
    <ClCompile Include="..\ConsoleControl.cpp" />
    <ClCompile Include="..\ConsoleInputThread.cpp" />
    <ClCompile Include="..\ConsoleWindow.cpp" />
    <ClCompile Include="..\InputServices.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\SystemConfigurationProvider.cpp" />
    <ClCompile Include="..\WindowMetrics.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\..\..\host\newdelete.hpp" />
    <ClInclude Include="..\AccessibilityNotifier.hpp" />
    <ClInclude Include="..\ConsoleControl.hpp" />
    <ClInclude Include="..\ConsoleInputThread.hpp" />
    <ClInclude Include="..\ConsoleWindow.hpp" />
    <ClInclude Include="..\InputServices.hpp" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\SystemConfigurationProvider.hpp" />
    <ClInclude Include="..\WindowMetrics.hpp" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
