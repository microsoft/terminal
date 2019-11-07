<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{CA5CAD1A-b11c-4ddb-a4fe-c3afae9b5506}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>TerminalAppLocalTests</RootNamespace>
    <ProjectName>LocalTests_TerminalApp</ProjectName>
    <TargetName>TerminalApp.LocalTests</TargetName>
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
    <ClInclude Include="JsonTestClass.h" />
  </ItemGroup>

  <!-- ========================= Cpp Files ======================== -->
  <ItemGroup>
    <ClCompile Include="SettingsTests.cpp" />
    <ClCompile Include="ProfileTests.cpp" />
    <ClCompile Include="ColorSchemeTests.cpp" />
    <ClCompile Include="KeyBindingsTests.cpp" />
    <ClCompile Include="TabTests.cpp" />
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

    <!-- If you don't reference these projects here, the
    _ConsoleGenerateAdditionalWinmdManifests step won't gather the winmd's -->
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalSettings\TerminalSettings.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalControl\TerminalControl.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalConnection\TerminalConnection.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalApp\TerminalApp.vcxproj" />
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

<!-- We actually can just straight up reference MUX here, it's fine -->
  <Import Project="..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets" Condition="Exists('..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets')" />
  <Import Project="..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview6.2\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets" Condition="Exists('..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview6.2\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets')" />

  <!-- This project will generate individual sxs manifests for each of our winrt libraries -->
  <Import Project="$(OpenConsoleDir)\build\rules\GenerateSxsManifestsFromWinmds.targets" />

  <!-- This is important: actually add the _LocalTestsGenerateCombinedManifests
    target to the list of targets to run. -->
  <PropertyGroup>
    <BeforeLinkTargets Condition="'$(WindowsTargetPlatformVersion)' &gt;= '10.0.18362.0'">
      $(BeforeLinkTargets);
      _LocalTestsGenerateCombinedManifests;
      _LocalTestsBuildAppxManifest;
      _LocalTestsCopyDependencies;
    </BeforeLinkTargets>
  </PropertyGroup>

  <!-- Step 1: Combine all our SxS manifests into a single SxS manifest. TAEF
  needs us to specify a single activation context at runtime, so we need a
  single file with all our types in it.-->
  <Target Name="_LocalTestsGenerateCombinedManifests"
          Inputs="@(_ConsoleWinmdManifest)"
          Outputs="$(OutDir)$(TargetName).manifest"
          DependsOnTargets="_ConsoleGenerateAdditionalWinmdManifests">

    <Exec Command="mt.exe -manifest @(_ConsoleWinmdManifest, ' -manifest ') -out:$(OutDir)$(TargetName).manifest" />
  </Target>

  <!-- Step 2: Take our combined SxS manifest, and use it to build an
  Appxmanifest.xml. We'll use the Appxmanifest.prototype.xml in this project's
  directory as a base, and the script will tak all our activatableClasses and
  turn them into appxmanifest-compatible Extensions -->
  <Target Name="_LocalTestsBuildAppxManifest"
          Inputs="$(OutDir)$(TargetName).manifest"
          Outputs="$(OutDir)$(TargetName).AppxManifest.xml"
          DependsOnTargets="_LocalTestsGenerateCombinedManifests">

    <Exec Command="powershell.exe -noprofile –ExecutionPolicy Unrestricted $(OpenConsoleDir)\tools\GenerateAppxFromManifest.ps1 -SxSManifest $(OutDir)$(TargetName).manifest -AppxManifestPrototype $(TargetName).AppxManifest.prototype.xml -OutPath $(OutDir)$(TargetName).AppxManifest.xml" />

  </Target>

  <!-- Step 3: Manually copy all our dependent DLLs into our OutDir. For SxS
  activation to work, they all need to be in the same directory as our test dll.
  This is using code that's heavliy cribbed from WindowsTerminal.vcxproj, which
  is already cribbed from the GetPackagingOutputs in
  Microsoft.*.AppxPackage.targets. We're filtering this list down to the dlls,
  pris and xbfs, because this list _can_ contain directories, which will make
  the Copy task explode. -->

  <PropertyGroup>
    <_ContinueOnError Condition="'$(BuildingProject)' == 'true'">true</_ContinueOnError>
    <_ContinueOnError Condition="'$(BuildingProject)' != 'true'">false</_ContinueOnError>
  </PropertyGroup>

  <!-- First gather the files... -->
  <Target Name="MyGetPackagingOutputs" Returns="@(MyPackagingOutputs)">
    <MSBuild
      Projects="@(ProjectReferenceWithConfiguration)"
      Targets="GetPackagingOutputs"
      BuildInParallel="$(BuildInParallel)"
      Properties="%(ProjectReferenceWithConfiguration.SetConfiguration); %(ProjectReferenceWithConfiguration.SetPlatform)"
      Condition="'@(ProjectReferenceWithConfiguration)' != ''
                 and '%(ProjectReferenceWithConfiguration.BuildReference)' == 'true'
                 and '%(ProjectReferenceWithConfiguration.ReferenceOutputAssembly)' == 'true'"
      ContinueOnError="$(_ContinueOnError)">
      <Output TaskParameter="TargetOutputs" ItemName="_PackagingOutputsFromOtherProjects"/>
    </MSBuild>

    <ItemGroup>
      <MyPackagingOutputs Include="@(_PackagingOutputsFromOtherProjects)" Condition="'%(Extension)'=='.dll' Or '%(Extension)'=='.pri' Or '%(Extension)'=='.xbf'" />
    </ItemGroup>
  </Target>

  <!-- Then copy the files to our outdir -->
  <Target Name="_LocalTestsCopyDependencies"
          DependsOnTargets="MyGetPackagingOutputs">

    <Copy SourceFiles="@(MyPackagingOutputs)"
          SkipUnchangedFiles="true"
          DestinationFolder="$(OutDir)"
    />
  </Target>

</Project>
