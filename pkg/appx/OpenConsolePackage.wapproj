<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="15.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="..\..\common.openconsole.props" Condition="'$(OpenConsoleDir)'==''" />
  <Import Project="$(OpenConsoleDir)src\wap-common.build.pre.props" />

  <PropertyGroup Label="Configuration">
    <TargetPlatformVersion>10.0.17763.0</TargetPlatformVersion>
    <TargetPlatformMinVersion>10.0.17134.0</TargetPlatformMinVersion>
    <!--
    These two properties are very important!
    Without them, msbuild will stomp MinVersion and MaxVersionTested in the
    Package.appxmanifest and replace them with whatever our values for
    TargetPlatformMinVersion and TargetPlatformVersion are.
     -->
    <AppxOSMinVersionReplaceManifestVersion>false</AppxOSMinVersionReplaceManifestVersion>
    <AppxOSMaxVersionTestedReplaceManifestVersion>false</AppxOSMaxVersionTestedReplaceManifestVersion>

    <!-- In a WAP project, this suppresses a spurious dependency on the non-Desktop VCLibs. -->
    <IncludeGetResolvedSDKReferences>false</IncludeGetResolvedSDKReferences>

    <!-- Suppress the inclusion of Windows.winmd in the appx. -->
    <SkipUnionWinmd>true</SkipUnionWinmd>
  </PropertyGroup>

  <PropertyGroup>
    <ProjectGuid>2D310963-F3E0-4EE5-8AC6-FBC94DCC3310</ProjectGuid>

    <EntryPointExe>OpenConsole.exe</EntryPointExe>
    <EntryPointProjectUniqueName>..\..\src\host\exe\Host.EXE.vcxproj</EntryPointProjectUniqueName>
  </PropertyGroup>

  <PropertyGroup>
    <GenerateAppInstallerFile>False</GenerateAppInstallerFile>
  </PropertyGroup>

  <PropertyGroup Condition="!Exists('OpenConsolePackage_TemporaryKey.pfx')">
    <AppxPackageSigningEnabled>false</AppxPackageSigningEnabled>
    <AppxBundle>Never</AppxBundle>
  </PropertyGroup>

  <PropertyGroup Condition="Exists('OpenConsolePackage_TemporaryKey.pfx')">
    <AppxPackageSigningEnabled>true</AppxPackageSigningEnabled>
    <AppxAutoIncrementPackageRevision>False</AppxAutoIncrementPackageRevision>
    <PackageCertificateKeyFile>OpenConsolePackage_TemporaryKey.pfx</PackageCertificateKeyFile>
  </PropertyGroup>

  <ItemGroup Condition="Exists('OpenConsolePackage_TemporaryKey.pfx')">
    <None Include="OpenConsolePackage_TemporaryKey.pfx" />
  </ItemGroup>

  <ItemGroup>
    <AppxManifest Include="Package.appxmanifest">
      <SubType>Designer</SubType>
    </AppxManifest>
  </ItemGroup>

  <ItemGroup>
    <Content Include="images\LockScreenLogo.scale-200.png" />
    <Content Include="images\Square150x150Logo.scale-200.png" />
    <Content Include="images\Square44x44Logo.scale-200.png" />
    <Content Include="images\Square44x44Logo.targetsize-16_altform-unplated.png" />
    <Content Include="images\Square44x44Logo.targetsize-24_altform-unplated.png" />
    <Content Include="images\Square44x44Logo.targetsize-32_altform-unplated.png" />
    <Content Include="images\Square44x44Logo.targetsize-48_altform-unplated.png" />
    <Content Include="images\Square44x44Logo.targetsize-256_altform-unplated.png" />
    <Content Include="images\StoreLogo.png" />
    <Content Include="images\Wide310x150Logo.scale-200.png" />
  </ItemGroup>

  <Import Project="$(OpenConsoleDir)src\wap-common.build.post.props" />

  <ItemGroup>
    <ProjectReference Include="..\..\src\host\exe\Host.EXE.vcxproj" />
    <ProjectReference Include="..\..\src\propsheet\propsheet.vcxproj" />
  </ItemGroup>

  <Target Name="OpenConsoleStompSourceProjectForWapProject" BeforeTargets="_ConvertItems">
    <ItemGroup>
      <_TemporaryFilteredWapProjOutput Include="@(_FilteredNonWapProjProjectOutput)" />
      <_FilteredNonWapProjProjectOutput Remove="@(_FilteredNonWapProjProjectOutput)" />
      <_FilteredNonWapProjProjectOutput Include="@(_TemporaryFilteredWapProjOutput)">
        <SourceProject></SourceProject> <!-- Blank SourceProject, which WapProj uses to name subdirectories. -->
      </_FilteredNonWapProjProjectOutput>
    </ItemGroup>
  </Target>
</Project>
