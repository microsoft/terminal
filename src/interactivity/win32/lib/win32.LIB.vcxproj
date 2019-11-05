<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{06EC74CB-9A12-429C-B551-8532EC964726}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>win32</RootNamespace>
    <ProjectName>InteractivityWin32</ProjectName>
    <TargetName>ConInteractivityWin32Lib</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>_WINDLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="..\AccessibilityNotifier.cpp" />
    <ClCompile Include="..\Clipboard.cpp" />
    <ClCompile Include="..\ConsoleControl.cpp" />
    <ClCompile Include="..\ConsoleInputThread.cpp" />
    <ClCompile Include="..\ConsoleKeyInfo.cpp" />
    <ClCompile Include="..\Find.cpp" />
    <ClCompile Include="..\Icon.cpp" />
    <ClCompile Include="..\InputServices.cpp" />
    <ClCompile Include="..\Menu.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\screenInfoUiaProvider.cpp" />
    <ClCompile Include="..\SystemConfigurationProvider.cpp" />
    <ClCompile Include="..\uiaTextRange.cpp" />
    <ClCompile Include="..\Window.cpp" />
    <ClCompile Include="..\WindowDpiApi.cpp" />
    <ClCompile Include="..\WindowIme.cpp" />
    <ClCompile Include="..\WindowIo.cpp" />
    <ClCompile Include="..\WindowMetrics.cpp" />
    <ClCompile Include="..\WindowProc.cpp" />
    <ClCompile Include="..\windowtheme.cpp" />
    <ClCompile Include="..\windowUiaProvider.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\AccessibilityNotifier.hpp" />
    <ClInclude Include="..\Clipboard.hpp" />
    <ClInclude Include="..\ConsoleControl.hpp" />
    <ClInclude Include="..\ConsoleInputThread.hpp" />
    <ClInclude Include="..\ConsoleKeyInfo.hpp" />
    <ClInclude Include="..\Find.h" />
    <ClInclude Include="..\Icon.hpp" />
    <ClInclude Include="..\InputServices.hpp" />
    <ClInclude Include="..\Menu.hpp" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\resource.h" />
    <ClInclude Include="..\screenInfoUiaProvider.hpp" />
    <ClInclude Include="..\SystemConfigurationProvider.hpp" />
    <ClInclude Include="..\uiaTextRange.hpp" />
    <ClInclude Include="..\Window.hpp" />
    <ClInclude Include="..\WindowDpiApi.hpp" />
    <ClInclude Include="..\WindowIme.hpp" />
    <ClInclude Include="..\WindowMetrics.hpp" />
    <ClInclude Include="..\WindowIo.hpp" />
    <ClInclude Include="..\windowtheme.hpp" />
    <ClInclude Include="..\windowUiaProvider.hpp" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..\..\..\inc;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
