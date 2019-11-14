<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <Import Project="..\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />

  <Import Project="$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.props" Condition="Exists('$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.props')" />

  <PropertyGroup Label="Globals">
    <!-- 17134 is RS4, 17763 is RS5, 18362 is 19H1 -->
    <WindowsTargetPlatformVersion Condition="'$(WindowsTargetPlatformVersion)' == ''">10.0.18362.0</WindowsTargetPlatformVersion>
    <WindowsTargetPlatformMinVersion Condition="'$(WindowsTargetPlatformMinVersion)' == ''">10.0.18362.0</WindowsTargetPlatformMinVersion>
    <CppWinRTEnabled>true</CppWinRTEnabled>
    <CppWinRTOptimized>true</CppWinRTOptimized>
    <DefaultLanguage>en-US</DefaultLanguage>
    <MinimumVisualStudioVersion>14.0</MinimumVisualStudioVersion>
    <ApplicationTypeRevision>10.0</ApplicationTypeRevision>
  </PropertyGroup>

  <!-- These settings tell MsBuild to treat the project as a "Universal Windows"
       application. This includes doing things like creating a seperate
       subdirectory for our binary output, and making sure that a wapproj looks
       at the winmd we build to generate type info.
       In general, cppwinrt projects all want this.
  -->
  <PropertyGroup Condition="'$(OpenConsoleUniversalApp)'!='false'">
    <MinimalCoreWin>true</MinimalCoreWin>
    <AppContainerApplication>true</AppContainerApplication>
    <ApplicationType>Windows Store</ApplicationType>
  </PropertyGroup>

  <!-- This is magic that tells msbuild to link against the Desktop platform
       instead of the App platform. This you definitely want, because we're not
       building a true universal "app", we're building a desktop application
       with xaml. APIs like CreatePseudoConsole won't always be linkable without Desktop platform,
	   but we now carry our own copy of them in the winconpty library which works everywhere.
       For future reference, _VC_Target_Library_Platform can be configured to say which CRT to link against.
       Now that this doesn't say anything, the implicit is 'Store' which links against the App CRT.
       The alternative is 'Desktop' which links against the usual CRT redistributable.
       Linking against the App CRT will require the VCRT forwarders to be dropped next to the final binary for
       desktop situations, but it will let non-desktop situations work since there is no redist off desktop.
       The forwarders can be found at https://github.com/microsoft/vcrt-forwarders and are already included
       in our CascadiaPackage project.
  -->
  <PropertyGroup>
    <_NoWinAPIFamilyApp>true</_NoWinAPIFamilyApp>
    <GenerateProjectSpecificOutputFolder>false</GenerateProjectSpecificOutputFolder>
  </PropertyGroup>

  <PropertyGroup Label="Configuration">
    <GenerateManifest>false</GenerateManifest>
  </PropertyGroup>

  <Import Project="$(MSBuildThisFileDirectory)common.build.pre.props" />

  <!-- Overrides for common build settings -->
  <ItemDefinitionGroup>
    <ClCompile>
      <!-- C++/WinRT hardcodes pch.h -->
      <PrecompiledHeader>Use</PrecompiledHeader>
      <PrecompiledHeaderFile>pch.h</PrecompiledHeaderFile>
      <PrecompiledHeaderOutputFile>$(IntDir)pch.pch</PrecompiledHeaderOutputFile>

      <!-- All new code should be in non-permissive mode. Big objects for C++/WinRT. -->
      <AdditionalOptions>%(AdditionalOptions) /permissive- /bigobj /Zc:twoPhase-</AdditionalOptions>
      <DisableSpecificWarnings>28204;%(DisableSpecificWarnings)</DisableSpecificWarnings>

      <AdditionalUsingDirectories>$(WindowsSDK_WindowsMetadata);$(AdditionalUsingDirectories)</AdditionalUsingDirectories>
    </ClCompile>
    <Link>
      <SubSystem Condition="'%(SubSystem)'==''">Console</SubSystem>
      <GenerateWindowsMetadata>true</GenerateWindowsMetadata>
    </Link>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(OpenConsoleDir)\src\cascadia\WinRTUtils\inc;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AdditionalDependencies>WindowsApp.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <AppContainer>false</AppContainer>
    </Link>
  </ItemDefinitionGroup>

  <!-- DLLs -->
  <ItemDefinitionGroup Condition="'$(ConfigurationType)'=='DynamicLibrary'">
    <ClCompile>
      <PreprocessorDefinitions>_WINRT_DLL;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link>
      <ModuleDefinitionFile>$(ProjectName).def</ModuleDefinitionFile>
    </Link>
  </ItemDefinitionGroup>

</Project>

