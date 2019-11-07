<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{CA5CAD1A-9333-4D05-B12A-1905CBF112F9}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>TerminalAppUnitTests</RootNamespace>
    <ProjectName>UnitTests_TerminalApp</ProjectName>
    <TargetName>Terminal.App.Unit.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <WindowsTargetPlatformMinVersion>10.0.18362.0</WindowsTargetPlatformMinVersion>
    <WindowsTargetPlatformVersion>10.0.18362.0</WindowsTargetPlatformVersion>
    <!-- We'll manage our own OutDir/IntDir -->
    <NoOutputRedirection>true</NoOutputRedirection>
  </PropertyGroup>

  <PropertyGroup>
  <!-- Manually change our outdir to be in a subdirectory. We don't really want
    to put our output in the bin root, because if we do, we'll copy
    TerminalApp.winmd to the bin root, and then every subsequent mdmerge step
    (in _any_ cppwinrt project) will automatically try to pick up
    TerminalApp.winmd as a dependency (which is just wrong). This MUST be done
    before importing common.build.pre.props -->
    <OutDir>$(SolutionDir)bin\$(Platform)\$(Configuration)\$(ProjectName)\</OutDir>
    <IntDir>$(SolutionDir)obj\$(Platform)\$(Configuration)\$(ProjectName)\</IntDir>
  </PropertyGroup>

  <Import Project="$(SolutionDir)\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(OpenConsoleDir)\src\common.build.pre.props" />

  <!-- ========================= Headers ======================== -->
  <ItemGroup>
    <ClInclude Include="precomp.h" />
  </ItemGroup>

  <!-- ========================= Cpp Files ======================== -->
  <ItemGroup>
    <ClCompile Include="JsonTests.cpp" />
    <ClCompile Include="DynamicProfileTests.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <!-- You _NEED_ to include this file and the jsoncpp IncludePath (below) if
    you want to use jsoncpp -->
    <ClCompile Include="$(OpenConsoleDir)\dep\jsoncpp\jsoncpp.cpp">
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>

  <!-- ========================= Project References ======================== -->
  <ItemGroup>
    <ProjectReference Include="$(OpenConsoleDir)\src\cascadia\TerminalApp\lib\TerminalAppLib.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)\src\types\lib\types.vcxproj" />
  </ItemGroup>

  <!-- ========================= Globals ======================== -->

  <!-- ====================== Compiler & Linker Flags ===================== -->
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;$(OpenConsoleDir)\dep\jsoncpp\json;$(OpenConsoleDir)src\inc;$(OpenConsoleDir)src\inc\test;$(WinRT_IncludePath)\..\cppwinrt\winrt;"$(OpenConsoleDir)\src\cascadia\TerminalApp\lib\Generated Files";%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PrecompiledHeaderFile>precomp.h</PrecompiledHeaderFile>

      <!-- Manually disable unreachable code warning, because jconcpp has a ton of that. -->
      <DisableSpecificWarnings>4702;%(DisableSpecificWarnings)</DisableSpecificWarnings>
    </ClCompile>
    <Link>
      <AdditionalDependencies>WindowsApp.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <PropertyGroup>
    <GenerateManifest>true</GenerateManifest>
    <EmbedManifest>true</EmbedManifest>
  </PropertyGroup>
  <ItemGroup>
    <Manifest Include="TerminalApp.Unit.Tests.manifest" />
  </ItemGroup>

  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(OpenConsoleDir)src\common.build.post.props" />
  <Import Project="$(OpenConsoleDir)src\common.build.tests.props" />

  <PropertyGroup>
    <_CppWinrtBinRoot>&quot;$(OpenConsoleDir)$(Platform)\$(Configuration)\&quot;</_CppWinrtBinRoot>
    <!-- From Microsoft.UI.Xaml.targets -->
    <Native-Platform Condition="'$(Platform)' == 'Win32'">x86</Native-Platform>
    <Native-Platform Condition="'$(Platform)' != 'Win32'">$(Platform)</Native-Platform>
    <_MUXBinRoot>&quot;$(OpenConsoleDir)packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\runtimes\win10-$(Native-Platform)\native\&quot;</_MUXBinRoot>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <PreBuildEvent>
      <!-- Manually copy the Windows Terminal manifest to our project directory,
      so we only need to have one version for the entire solution. -->
      <Command>
        (xcopy /Y &quot;$(OpenConsoleDir)src\cascadia\WindowsTerminal\WindowsTerminal.manifest&quot; &quot;$(OpenConsoleDir)src\cascadia\ut_app\TerminalApp.Unit.Tests.manifest*&quot; )
      </Command>
    </PreBuildEvent>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup>
    <PostBuildEvent>
      <!-- Manually copy A few needed things to our outdir.
      1. Our manifest, because some tests that can run unpackaged will need that
         to activate winrt types.
      2. Our AppManifest, for tests that _can't_ run unpackaged. Taef will use
         this during execution to create a temp package to run the tests in.
      3. All our dependent dlls, for any cppwinrt projects we have. We'll need
         them adjacent if we hope to activate any types contained in them. This
         also includes MUX manually, as the MUX targets from the nuget package
         don't work on this project :/
      -->
      <Command>
        echo OutDir=$(OutDir)
        (xcopy /Y &quot;$(OpenConsoleDir)src\cascadia\ut_app\TerminalApp.Unit.Tests.manifest&quot; &quot;$(OutDir)\TerminalApp.Unit.Tests.manifest*&quot; )
        (xcopy /Y &quot;$(OpenConsoleDir)src\cascadia\ut_app\TerminalApp.Unit.Tests.AppxManifest.xml&quot; &quot;$(OutDir)\TerminalApp.Unit.Tests.AppxManifest.xml*&quot; )
      </Command>
    </PostBuildEvent>
  </ItemDefinitionGroup>

  <!-- Import this set of targets that fixes a VS bug that manifests when using
  the TerminalAppLib project -->
  <Import Project="../TerminalApp/FixVisualStudioBug.targets" />

</Project>
