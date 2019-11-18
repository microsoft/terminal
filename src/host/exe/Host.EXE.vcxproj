<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{9CBD7DFA-1754-4A9D-93D7-857A9D17CB1B}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>HostEXE</RootNamespace>
    <ProjectName>Host.EXE</ProjectName>
    <TargetName>OpenConsole</TargetName>
    <ConfigurationType>Application</ConfigurationType>
  </PropertyGroup>
  <Import Project="..\..\common.build.pre.props" />
  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="resource.h" />
  </ItemGroup>
  <ItemGroup>
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <ClCompile Include="..\exemain.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\buffer\out\lib\bufferout.vcxproj">
      <Project>{0cf235bd-2da0-407e-90ee-c467e8bbc714}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\interactivity\base\lib\InteractivityBase.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8562ec964846}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\interactivity\win32\lib\win32.LIB.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8532ec964726}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\internal\internal.vcxproj">
      <Project>{ef3e32a7-5ff6-42b4-b6e2-96cd7d033f00}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\propslib\propslib.vcxproj">
      <Project>{345fd5a4-b32b-4f29-bd1c-b033bd2c35cc}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\base\lib\base.vcxproj">
      <Project>{af0a096a-8b3a-4949-81ef-7df8f0fee91f}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\dx\lib\dx.vcxproj">
      <Project>{48d21369-3d7b-4431-9967-24e81292cf62}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\gdi\lib\gdi.vcxproj">
      <Project>{1c959542-bac2-4e55-9a6d-13251914cbb9}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\vt\lib\vt.vcxproj">
      <Project>{990f2657-8580-4828-943f-5dd657d11842}</Project>
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
    <ResourceCompile Include="Host.EXE.rc" />
  </ItemGroup>
  <ItemGroup>
    <Manifest Include="openconsole.exe.manifest" />
  </ItemGroup>
  <PropertyGroup>
    <EmbedManifest>
    </EmbedManifest>
    <GenerateManifest>true</GenerateManifest>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AllowIsolation>true</AllowIsolation>
    </Link>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="..\..\common.build.post.props" />
</Project>
