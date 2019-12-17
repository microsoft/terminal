<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="$(MSBuildThisFileDirectory)common.build.post.props" />

  <!-- For Cascadia projects, we want to copy the output dll up a directory
       from where it was binplaced.
       e.g. TerminalApp.vcxproj builds OpenConsole/x64/Debug/TerminalApp/TerminalApp.dll
       this should be copied to OpenConsole/x64/Debug/TerminalApp.dll
       Otherwise, the wapproj won't be able to find it properly.
       Additionally, the wapproj expects the x86 version to not include the
       platform in the path, go figure.
       A project can request to not do this step by setting DontCopyOutput to true.
  -->
  <ItemDefinitionGroup Condition="'$(NoOutputRedirection)'=='true' And '$(ConfigurationType)'=='DynamicLibrary' And '$(DontCopyOutput)'!='true'">
    <PostBuildEvent Condition="'$(Platform)'!='Win32'">
      <Command>
        echo OutDir=$(OutDir)
        (xcopy /Y &quot;$(OutDir)$(ProjectName).dll&quot; &quot;$(OpenConsoleDir)$(Platform)\$(Configuration)\$(ProjectName).dll*&quot; )
        (xcopy /Y &quot;$(OutDir)$(ProjectName)FullPDB.pdb&quot; &quot;$(OpenConsoleDir)$(Platform)\$(Configuration)\$(ProjectName)FullPDB.pdb*&quot; )
      </Command>
    </PostBuildEvent>
    <PostBuildEvent Condition="'$(Platform)'=='Win32'">
      <Command>
        echo OutDir=$(OutDir)
        (xcopy /Y &quot;$(OutDir)$(ProjectName).dll&quot; &quot;$(OpenConsoleDir)$(Configuration)\$(ProjectName).dll*&quot; )
        (xcopy /Y &quot;$(OutDir)$(ProjectName)FullPDB.pdb&quot; &quot;$(OpenConsoleDir)$(Configuration)\$(ProjectName)FullPDB.pdb*&quot; )
      </Command>
    </PostBuildEvent>
  </ItemDefinitionGroup>

  <!--
    For whatever reason, GENERATEPROJECTPRIFILE is incapable of creating this
    directory on its own, it just assumes it exists.

    Also make the directory w/o the platform, because the x86 build in CI will
    try to place the pri files into
    C:\BA\3126\s\Release\$(ProjectName)\$(ProjectName).pri (for example)
  -->
  <ItemDefinitionGroup Condition="'$(Platform)'!='Win32'">
    <PreBuildEvent>
      <Command>
        if not exist &quot;$(OpenConsoleDir)$(Platform)\$(Configuration)\$(ProjectName)&quot; mkdir &quot;$(OpenConsoleDir)$(Platform)\$(Configuration)\$(ProjectName)&quot;
      </Command>
    </PreBuildEvent>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Platform)'=='Win32'">
    <PreBuildEvent>
      <Command>
        if not exist &quot;$(OpenConsoleDir)$(Configuration)\$(ProjectName)&quot; mkdir &quot;$(OpenConsoleDir)\$(Configuration)\$(ProjectName)&quot;
      </Command>
    </PreBuildEvent>
  </ItemDefinitionGroup>

  <ImportGroup Label="ExtensionTargets">
    <Import Project="$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.targets" Condition="Exists('$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.targets')" />
  </ImportGroup>
  <Target Name="EnsureNuGetPackageBuildImports" BeforeTargets="PrepareForBuild">
    <PropertyGroup>
      <ErrorText>This project references NuGet package(s) that are missing on this computer. Use NuGet Package Restore to download them.  For more information, see http://go.microsoft.com/fwlink/?LinkID=322105. The missing file is {0}.</ErrorText>
    </PropertyGroup>
    <Error Condition="!Exists('$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.props')" Text="$([System.String]::Format('$(ErrorText)', '$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.props'))" />
    <Error Condition="!Exists('$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.targets')" Text="$([System.String]::Format('$(ErrorText)', '$(OpenConsoleDir)\packages\Microsoft.Windows.CppWinRT.2.0.190730.2\build\native\Microsoft.Windows.CppWinRT.targets'))" />
  </Target>
</Project>
