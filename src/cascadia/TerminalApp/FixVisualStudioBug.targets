<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!--
    This is a terrible, terrible rule. There exists a bug in Visual Studio 2019 16.2 and 16.3 previews
    where ResolveAssemblyReferences will try and fail to parse a .lib when it produces a .winmd.
    To fix that, we have to _temporarily_ replace the %(Implementation) on any winmd-producing
    static library references with the empty string so as to make ResolveAssemblyReferences
    not try to read it.

    Upstream problem report:
    https://developercommunity.visualstudio.com/content/problem/629524/static-library-reference-causes-there-was-a-proble.html
  -->
  <Target Name="_RemoveTerminalAppLibImplementationFromReference" BeforeTargets="ResolveAssemblyReferences">
    <ItemGroup>
      <_TerminalAppLibProjectReference Include="@(_ResolvedProjectReferencePaths)" Condition="'%(Filename)' == 'TerminalApp'" />
      <_ResolvedProjectReferencePaths Remove="@(_TerminalAppLibProjectReference)" />
      <_ResolvedProjectReferencePaths Include="@(_TerminalAppLibProjectReference)">
        <Implementation />
      </_ResolvedProjectReferencePaths>
    </ItemGroup>
  </Target>

  <Target Name="_RestoreTerminalAppLibImplementationFromReference" AfterTargets="ResolveAssemblyReferences">
    <ItemGroup>
      <_ResolvedProjectReferencePaths Remove="@(_TerminalAppLibProjectReference)" />
      <_ResolvedProjectReferencePaths Include="@(_TerminalAppLibProjectReference)" />
    </ItemGroup>
  </Target>
  <!-- End "terrible, terrible rule" -->
</Project>
