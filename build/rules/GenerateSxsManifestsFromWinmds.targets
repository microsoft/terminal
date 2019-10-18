<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="16.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <BeforeLinkTargets Condition="'$(WindowsTargetPlatformVersion)' &gt;= '10.0.18362.0'">
      $(BeforeLinkTargets);
      _ConsoleGenerateAdditionalWinmdManifests;
    </BeforeLinkTargets>
  </PropertyGroup>

  <Target Name="_ConsoleMapWinmdsToManifestFiles" DependsOnTargets="ResolveAssemblyReferences">
    <ItemGroup>
      <!-- For each non-system .winmd file in References, generate a .manifest in IntDir for it. -->
      <_ConsoleWinmdManifest Include="@(ReferencePath->'$(IntDir)\%(FileName).manifest')" Condition="'%(ReferencePath.IsSystemReference)' != 'true' and '%(ReferencePath.WinMDFile)' == 'true' and '%(ReferencePath.ReferenceSourceTarget)' == 'ResolveAssemblyReference' and '%(ReferencePath.Implementation)' != ''">
        <WinMDPath>%(ReferencePath.FullPath)</WinMDPath>
        <Implementation>%(ReferencePath.Implementation)</Implementation>
      </_ConsoleWinmdManifest>
      <!-- For each referenced project that _produces_ a winmd, generate a temporary item that maps to
           the winmd, and use that temporary item to generate a .manifest in IntDir for it.
           We don't set Implementation here because it's inherited from the _ResolvedNativeProjectReferencePaths. -->
      <_ConsoleWinmdProjectReference Condition="'%(_ResolvedNativeProjectReferencePaths.ProjectType)' != 'StaticLibrary'" Include="@(_ResolvedNativeProjectReferencePaths-&gt;WithMetadataValue('FileType','winmd')-&gt;'%(RootDir)%(Directory)%(TargetPath)')" />
      <_ConsoleWinmdManifest Include="@(_ConsoleWinmdProjectReference->'$(IntDir)\%(FileName).manifest')">
        <WinMDPath>%(Identity)</WinMDPath>
      </_ConsoleWinmdManifest>
    </ItemGroup>
  </Target>

  <Target Name="_ConsoleGenerateAdditionalWinmdManifests"
          Inputs="@(_ConsoleWinmdManifest.WinMDPath)"
          Outputs="@(_ConsoleWinmdManifest)"
          DependsOnTargets="_ConsoleMapWinmdsToManifestFiles">

    <!-- This target is batched and a new Exec is spawned for each entry in _ConsoleWinmdManifest. -->
    <Exec Command="mt.exe -winmd:%(_ConsoleWinmdManifest.WinMDPath) -dll:%(_ConsoleWinmdManifest.Implementation) -out:%(_ConsoleWinmdManifest.Identity)" />

    <ItemGroup>
      <!-- Emit the generated manifest into the Link inputs. -->
      <Manifest Include="@(_ConsoleWinmdManifest)" />
    </ItemGroup>

  </Target>
</Project>
