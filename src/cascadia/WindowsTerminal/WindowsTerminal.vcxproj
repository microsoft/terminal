<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.props" Condition="Exists('..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.props')" />

  <PropertyGroup Label="Globals">
    <ProjectGuid>{CA5CAD1A-1754-4A9D-93D7-857A9D17CB1B}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>WindowsTerminal</RootNamespace>
    <ProjectName>WindowsTerminal</ProjectName>
    <TargetName>WindowsTerminal</TargetName>
    <ConfigurationType>Application</ConfigurationType>
    <OpenConsoleUniversalApp>false</OpenConsoleUniversalApp>
    <ApplicationType>Windows Store</ApplicationType>
    <!-- IMPORTANT! Xaml Islands only works on >= 17709 -->
    <!-- IMPORTANT! cppwinrt.pre.props specifies 17134 -->
    <WindowsTargetPlatformVersion>10.0.18362.0</WindowsTargetPlatformVersion>
    <!-- DON'T REDIRECT OUR OUTPUT -->
    <NoOutputRedirection>true</NoOutputRedirection>
  </PropertyGroup>

  <Import Project="..\..\..\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(OpenConsoleDir)src\cppwinrt.build.pre.props" />

  <ItemDefinitionGroup>
    <ClCompile>
      <SDLCheck>true</SDLCheck>
    </ClCompile>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(OpenConsoleDir)\src\inc;$(OpenConsoleDir)\dep;$(OpenConsoleDir)\dep\Console;$(OpenConsoleDir)\dep\Win32K;$(OpenConsoleDir)\dep\gsl\include;%(AdditionalIncludeDirectories);</AdditionalIncludeDirectories>
    </ClCompile>
    <ClCompile>
      <!-- Manually include the generated TerminalCore header's path, because
           adding a project reference will confuse msbuild, because TerminalCore
           isn't a dll, it's a lib, and cppwinrt won't include a lib's header -->
      <AdditionalIncludeDirectories>"$(OpenConsoleDir)src\cascadia\TerminalCore\lib\Generated Files";%(AdditionalIncludeDirectories);</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AdditionalDependencies>gdi32.lib;dwmapi.lib;Shcore.lib;UxTheme.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <PropertyGroup>
    <GenerateManifest>true</GenerateManifest>
    <EmbedManifest>true</EmbedManifest>
  </PropertyGroup>
  <!-- Source Files -->
  <ItemGroup>
    <ClInclude Include="pch.h" />
    <ClInclude Include="resource.h" />
    <ClInclude Include="AppHost.h" />
    <ClInclude Include="BaseWindow.h" />
    <ClInclude Include="IslandWindow.h" />
    <ClInclude Include="NonClientIslandWindow.h" />
    <ClInclude Include="WindowUiaProvider.hpp" />
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="pch.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="main.cpp" />
    <ClCompile Include="AppHost.cpp" />
    <ClCompile Include="IslandWindow.cpp" />
    <ClCompile Include="NonClientIslandWindow.cpp" />
    <ClCompile Include="WindowUiaProvider.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ResourceCompile Include="WindowsTerminal.rc" />
  </ItemGroup>
  <ItemGroup>
    <Manifest Include="WindowsTerminal.manifest" />
  </ItemGroup>
  <ItemGroup>
    <None Include="packages.config" />
  </ItemGroup>
  <!-- Dependencies -->
  <ItemGroup>
    <!-- Even though we do have proper recursive dependencies, we want to keep some of these here
         so that the AppX Manifest contains their activatable classes. -->
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalSettings\TerminalSettings.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalControl\TerminalControl.vcxproj" />
    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalConnection\TerminalConnection.vcxproj" />

    <ProjectReference Include="$(OpenConsoleDir)src\cascadia\TerminalApp\TerminalApp.vcxproj" />

    <ProjectReference Include="$(OpenConsoleDir)src\types\lib\types.vcxproj" />
  </ItemGroup>

  <!--
    This ItemGroup and the Globals PropertyGroup below it are required in order
    to enable F5 debugging for the unpackaged application
    -->
  <ItemGroup>
    <PropertyPageSchema Include="$(VCTargetsPath)$(LangID)\debugger_general.xml" />
    <PropertyPageSchema Include="$(VCTargetsPath)$(LangID)\debugger_local_windows.xml" />
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
  </PropertyGroup>

  <Import Project="$(OpenConsoleDir)src\cppwinrt.build.post.props" />

  <Import Project="..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets" Condition="Exists('..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets')" />
  <Import Project="..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets" Condition="Exists('..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets')" />
  <Import Project="..\..\..\packages\Microsoft.VCRTForwarders.140.1.0.1-rc\build\native\Microsoft.VCRTForwarders.140.targets" Condition="Exists('..\..\..\packages\Microsoft.VCRTForwarders.140.1.0.1-rc\build\native\Microsoft.VCRTForwarders.140.targets')" />
  <Target Name="EnsureNuGetPackageBuildImports" BeforeTargets="PrepareForBuild">
    <PropertyGroup>
      <ErrorText>This project references NuGet package(s) that are missing on this computer. Use NuGet Package Restore to download them.  For more information, see http://go.microsoft.com/fwlink/?LinkID=322105. The missing file is {0}.</ErrorText>
    </PropertyGroup>
    <Error Condition="!Exists('..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets')" Text="$([System.String]::Format('$(ErrorText)', '..\..\..\packages\Microsoft.UI.Xaml.2.2.191021001-prerelease\build\native\Microsoft.UI.Xaml.targets'))" />
    <Error Condition="!Exists('..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.props')" Text="$([System.String]::Format('$(ErrorText)', '..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.props'))" />
    <Error Condition="!Exists('..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets')" Text="$([System.String]::Format('$(ErrorText)', '..\..\..\packages\Microsoft.Toolkit.Win32.UI.XamlApplication.6.0.0-preview7\build\native\Microsoft.Toolkit.Win32.UI.XamlApplication.targets'))" />
    <Error Condition="!Exists('..\..\..\packages\Microsoft.VCRTForwarders.140.1.0.1-rc\build\native\Microsoft.VCRTForwarders.140.targets')" Text="$([System.String]::Format('$(ErrorText)', '..\..\..\packages\Microsoft.VCRTForwarders.140.1.0.1-rc\build\native\Microsoft.VCRTForwarders.140.targets'))" />
  </Target>

  <!-- Override GetPackagingOutputs to roll up all our dependencies.
       This ensures that when the WAP packaging project asks what files go into
       the package, we tell it.
       This is a heavily stripped version of the one in Microsoft.*.AppxPackage.targets.
  -->
  <PropertyGroup>
    <_ContinueOnError Condition="'$(BuildingProject)' == 'true'">true</_ContinueOnError>
    <_ContinueOnError Condition="'$(BuildingProject)' != 'true'">false</_ContinueOnError>
  </PropertyGroup>
  <Target Name="GetPackagingOutputs" Returns="@(PackagingOutputs)">
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
      <PackagingOutputs Include="@(_PackagingOutputsFromOtherProjects)" />
    </ItemGroup>
  </Target>
  
  <Import Project="$(OpenConsoleDir)\build\rules\GenerateSxsManifestsFromWinmds.targets" />
</Project>
