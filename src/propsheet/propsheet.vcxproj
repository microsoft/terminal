<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{5D23E8E1-3C64-4CC1-A8F7-6861677F7239}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>propsheet</RootNamespace>
    <ProjectName>Propsheet.DLL</ProjectName>
    <TargetName>console</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="..\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="console.cpp" />
    <ClCompile Include="globals.cpp" />
    <ClCompile Include="dbcs.cpp" />
    <ClCompile Include="dll.cpp" />
    <ClCompile Include="fontdlg.cpp" />
    <ClCompile Include="init.cpp" />
    <ClCompile Include="misc.cpp" />
    <ClCompile Include="preview.cpp" />
    <ClCompile Include="PropSheetHandler.cpp" />
    <ClCompile Include="OptionsPage.cpp" />
    <ClCompile Include="LayoutPage.cpp" />
    <ClCompile Include="ColorsPage.cpp" />
    <ClCompile Include="ColorControl.cpp" />
    <ClCompile Include="TerminalPage.cpp" />
    <ClCompile Include="registry.cpp" />
    <ClCompile Include="util.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="console.h" />
    <ClInclude Include="dialogs.h" />
    <ClInclude Include="globals.h" />
    <ClInclude Include="font.h" />
    <ClInclude Include="fontdlg.h" />
    <ClInclude Include="OptionsPage.h" />
    <ClInclude Include="LayoutPage.h" />
    <ClInclude Include="ColorsPage.h" />
    <ClInclude Include="ColorControl.h" />
    <ClInclude Include="TerminalPage.h" />
    <ClInclude Include="menu.h" />
    <ClInclude Include="precomp.h" />
  </ItemGroup>
  <ItemGroup>
    <ResourceCompile Include="console.rc" />
    <MessageCompile Include="strid.mc" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\propslib\propslib.vcxproj">
      <Project>{345fd5a4-b32b-4f29-bd1c-b033bd2c35cc}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <None Include="console.def" />
  </ItemGroup>
  <PropertyGroup>
    <EmbedManifest>
    </EmbedManifest>
    <GenerateManifest>true</GenerateManifest>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <Link>
      <AdditionalDependencies>comctl32.lib;RuntimeObject.lib;Shlwapi.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <ModuleDefinitionFile>console.def</ModuleDefinitionFile>
      <AdditionalManifestDependencies>type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'</AdditionalManifestDependencies>
    </Link>
    <ClCompile>
      <AdditionalIncludeDirectories>$(IntermediateOutputPath);..\inc;..\host;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <ResourceCompile>
      <AdditionalIncludeDirectories>$(IntermediateOutputPath);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ResourceCompile>
  </ItemDefinitionGroup>
  <Target Name="MessageCompile" Inputs="@(MessageCompile)" Outputs="$(IntermediateOutputPath)\%(MessageCompile.Filename).h" BeforeTargets="ClCompile">
    <Exec Command="mc.exe /h $(IntermediateOutputPath) /r $(IntermediateOutputPath) @(MessageCompile)" />
  </Target>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="..\common.build.post.props" />
</Project>
