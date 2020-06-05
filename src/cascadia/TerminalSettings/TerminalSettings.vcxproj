<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{CA5CAD1A-d7ec-4107-b7c6-79cb77ae2907}</ProjectGuid>
    <ProjectName>TerminalSettings</ProjectName>
    <RootNamespace>Microsoft.Terminal.Settings</RootNamespace>
    <!-- cppwinrt.build.pre.props depends on these settings: -->
    <!-- build a dll, not exe (Application) -->
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <SubSystem>Console</SubSystem>
    <!-- sets a bunch of Windows Universal properties -->
    <OpenConsoleUniversalApp>true</OpenConsoleUniversalApp>
    <!--
      DON'T REDIRECT OUR OUTPUT.
      Setting this will tell cppwinrt.build.post.props to copy our output from
      the default OutDir up one level, so the wapproj will be able to find it.
     -->
    <NoOutputRedirection>true</NoOutputRedirection>
  </PropertyGroup>

  <Import Project="..\..\..\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(OpenConsoleDir)src\cppwinrt.build.pre.props" />

  <ItemGroup>
    <ClInclude Include="pch.h" />
    <ClInclude Include="KeyChord.h">
      <DependentUpon>KeyChord.idl</DependentUpon>
    </ClInclude>
    <ClInclude Include="TerminalSettings.h">
      <DependentUpon>TerminalSettings.idl</DependentUpon>
    </ClInclude>
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="pch.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="KeyChord.cpp">
      <DependentUpon>KeyChord.idl</DependentUpon>
    </ClCompile>
    <ClCompile Include="TerminalSettings.cpp">
      <DependentUpon>TerminalSettings.idl</DependentUpon>
    </ClCompile>
    <ClCompile Include="$(GeneratedFilesDir)module.g.cpp" />
  </ItemGroup>
  <ItemGroup>
    <Midl Include="TerminalSettings.idl" />
    <Midl Include="ICoreSettings.idl" />
    <Midl Include="IControlSettings.idl" />
    <Midl Include="KeyChord.idl" />
    <Midl Include="IKeyBindings.idl" />
  </ItemGroup>
  <ItemGroup>
    <None Include="packages.config" />
    <None Include="TerminalSettings.def" />
  </ItemGroup>

  <Import Project="$(OpenConsoleDir)src\cppwinrt.build.post.props" />
</Project>
