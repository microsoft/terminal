<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!-- By default, binplace our output under the bin/ directory in the root of
       the project. The other non-cppwinrt projects are binplaced there as well.
       However, Cascadia projects want to be built into an appx, and the wapproj
       that consumes them will complain if you do this. So they'll set
       NoOutputRedirection before including this props file.
  -->
  <PropertyGroup Condition="'$(NoOutputRedirection)'!='true'">
    <OutputPath>$(SolutionDir)bin\$(Platform)\$(Configuration)\</OutputPath>
    <OutDir>$(SolutionDir)bin\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(SolutionDir)obj\$(Platform)\$(Configuration)\$(ProjectName)\</IntDir>
  </PropertyGroup>
  <PropertyGroup>
    <!-- This one is always set so that non-redirected projects can depend on it. -->
    <OpenConsoleCommonOutDir>$(SolutionDir)bin\$(Platform)\$(Configuration)\</OpenConsoleCommonOutDir>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />

  <ItemGroup Label="ProjectConfigurations">
  <ProjectConfiguration Include="AuditMode|Win32">
      <Configuration>AuditMode</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="AuditMode|x64">
      <Configuration>AuditMode</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="AuditMode|ARM64">
      <Configuration>AuditMode</Configuration>
      <Platform>ARM64</Platform>
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

  <PropertyGroup Label="Globals">
    <WindowsTargetPlatformVersion Condition="'$(WindowsTargetPlatformVersion)' == ''">10.0.18362.0</WindowsTargetPlatformVersion>
  </PropertyGroup>

  <!-- For ALL build types-->
  <PropertyGroup Label="Configuration">
    <PlatformToolset>v142</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
    <LinkIncremental>false</LinkIncremental>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <PrecompiledHeader>Use</PrecompiledHeader>
      <WarningLevel>Level4</WarningLevel>
      <TreatSpecificWarningsAsErrors>4189;4100;4242;4389;4244</TreatSpecificWarningsAsErrors>
      <!--<WarningLevel>EnableAllWarnings</WarningLevel>-->
      <TreatWarningAsError>true</TreatWarningAsError>
      <!-- disable warning on nameless structs (4201) -->
      <DisableSpecificWarnings>4201;4312;4467;%(DisableSpecificWarnings)</DisableSpecificWarnings>
      <PreprocessorDefinitions>_WINDOWS;EXTERNAL_BUILD;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <SDLCheck>true</SDLCheck>
      <PrecompiledHeaderFile>precomp.h</PrecompiledHeaderFile>
      <ProgramDataBaseFileName>$(IntDir)$(TargetName).pdb</ProgramDataBaseFileName>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
      <AdditionalIncludeDirectories>$(SolutionDir)\src\inc;$(SolutionDir)\dep;$(SolutionDir)\dep\Console;$(SolutionDir)\dep\Win32K;$(SolutionDir)\dep\gsl\include;$(SolutionDir)\dep\wil\include;%(AdditionalIncludeDirectories);</AdditionalIncludeDirectories>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <MinimalRebuild>false</MinimalRebuild>
      <RuntimeTypeInfo>false</RuntimeTypeInfo>
      <AdditionalOptions>/std:c++17 /utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <ResourceCompile>
      <PreprocessorDefinitions>EXTERNAL_BUILD;_UNICODE;UNICODE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ResourceCompile>
    <Link>
      <ProgramDatabaseFile>$(OutDir)$(TargetName)FullPDB.pdb</ProgramDatabaseFile>
      <SubSystem>Windows</SubSystem>
      <SetChecksum>true</SetChecksum>
    </Link>
  </ItemDefinitionGroup>

  <!-- For Debug ONLY -->
  <PropertyGroup Condition="'$(Configuration)'=='Debug'" Label="Configuration">
    <UseDebugLibraries>true</UseDebugLibraries>
    <LinkIncremental>false</LinkIncremental>
  </PropertyGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)'=='Debug'">
    <ClCompile>
      <Optimization>Disabled</Optimization>
      <PreprocessorDefinitions>_DEBUG;DBG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link>
      <GenerateDebugInformation>true</GenerateDebugInformation>
    </Link>
  </ItemDefinitionGroup>

  <!-- For Release ONLY -->
  <PropertyGroup Condition="'$(Configuration)'=='Release' Or '$(Configuration)'=='AuditMode'" Label="Configuration">
    <UseDebugLibraries>false</UseDebugLibraries>
    <WholeProgramOptimization>true</WholeProgramOptimization>
  </PropertyGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)'=='Release' Or '$(Configuration)'=='AuditMode'">
    <ClCompile>
      <Optimization>MaxSpeed</Optimization>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <PreprocessorDefinitions>NDEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <RuntimeTypeInfo>false</RuntimeTypeInfo>
    </ClCompile>
    <Link>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
    </Link>
  </ItemDefinitionGroup>

  <!-- For Win32 (x86) ONLY ... we use all defaults for AMD64. No def for those. -->
  <ItemDefinitionGroup Condition="'$(Platform)'=='Win32'">
    <ClCompile>
      <PreprocessorDefinitions>WIN32;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>

  <!-- For our Audit mode -->
  <PropertyGroup Condition="'$(Configuration)'=='AuditMode'">
    <CodeAnalysisRuleSet>$(SolutionDir)\src\StaticAnalysis.ruleset</CodeAnalysisRuleSet>
    <EnableCppCoreCheck>true</EnableCppCoreCheck>
    <RunCodeAnalysis>true</RunCodeAnalysis>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)'=='AuditMode'">
    <ClCompile>
      <EnablePREfast>true</EnablePREfast>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
