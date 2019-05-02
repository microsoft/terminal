<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="15.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Condition="'$(VisualStudioVersion)' == '' or '$(VisualStudioVersion)' &lt; '15.0'">
    <VisualStudioVersion>15.0</VisualStudioVersion>
  </PropertyGroup>

  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>x86</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|ARM64">
      <Configuration>Debug</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|ARM64">
      <Configuration>Release</Configuration>
      <Platform>ARM64</Platform>
    </ProjectConfiguration>
  </ItemGroup>

  <PropertyGroup>
    <WapProjPath Condition="'$(WapProjPath)'==''">$(MSBuildExtensionsPath)\Microsoft\DesktopBridge\</WapProjPath>
    <!-- Turn off the 6+MB Windows.winmd that's emitted into our package
         because something thinks it contains managed code. -->
    <SkipUnionWinmd>true</SkipUnionWinmd>
  </PropertyGroup>
  <Import Project="$(WapProjPath)\Microsoft.DesktopBridge.props" />

  <PropertyGroup>
    <TargetPlatformVersion>10.0.17134.0</TargetPlatformVersion>
    <TargetPlatformMinVersion>10.0.17134.0</TargetPlatformMinVersion>
    <DefaultLanguage>en-US</DefaultLanguage>
  </PropertyGroup>

</Project>
