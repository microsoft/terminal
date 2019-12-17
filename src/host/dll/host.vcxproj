<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{E437B604-3E98-4F40-A927-E173E818EA4B}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>host</RootNamespace>
    <ProjectName>Host.DLL</ProjectName>
    <TargetName>ConhostV2</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="..\main.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="resource.h" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\interactivity\base\lib\InteractivityBase.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8562ec964846}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\interactivity\win32\lib\win32.LIB.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8532ec964726}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\propslib\propslib.vcxproj">
      <Project>{345fd5a4-b32b-4f29-bd1c-b033bd2c35cc}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\base\lib\base.vcxproj">
      <Project>{af0a096a-8b3a-4949-81ef-7df8f0fee91f}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\gdi\lib\gdi.vcxproj">
      <Project>{1c959542-bac2-4e55-9a6d-13251914cbb9}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\server\lib\server.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820262}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\terminal\adapter\lib\adapter.vcxproj">
      <Project>{dcf55140-ef6a-4736-a403-957e4f7430bb}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\terminal\parser\lib\parser.vcxproj">
      <Project>{3ae13314-1939-4dfa-9c14-38ca0834050c}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\tsf\tsf.vcxproj">
      <Project>{2fd12fbb-1ddb-46d8-b818-1023c624caca}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\types\lib\types.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820263}</Project>
    </ProjectReference>
    <ProjectReference Include="..\lib\hostlib.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8562ec954746}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <None Include="ConhostV2.def" />
  </ItemGroup>
  <ItemGroup>
    <ResourceCompile Include="Host.DLL.rc" />
  </ItemGroup>
  <PropertyGroup>
    <EmbedManifest>
    </EmbedManifest>
    <GenerateManifest>true</GenerateManifest>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <PreprocessorDefinitions>%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
    <Link>
      <ModuleDefinitionFile>ConhostV2.def</ModuleDefinitionFile>
      <AllowIsolation>true</AllowIsolation>
      <AdditionalManifestDependencies>type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'</AdditionalManifestDependencies>
    </Link>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
