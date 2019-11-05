<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{531C23E7-4B76-4C08-8AAD-04164CB628C9}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>HostUnitTests</RootNamespace>
    <ProjectName>Host.Tests.Unit</ProjectName>
    <TargetName>Conhost.Unit.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="AliasTests.cpp" />
    <ClCompile Include="ApiRoutinesTests.cpp" />
    <ClCompile Include="AttrRowTests.cpp" />
    <ClCompile Include="ClipboardTests.cpp" />
    <ClCompile Include="ConsoleArgumentsTests.cpp" />
    <ClCompile Include="CommandLineTests.cpp" />
    <ClCompile Include="CodepointWidthDetectorTests.cpp" />
    <ClCompile Include="CommandListPopupTests.cpp" />
    <ClCompile Include="CommandNumberPopupTests.cpp" />
    <ClCompile Include="CopyFromCharPopupTests.cpp" />
    <ClCompile Include="CopyToCharPopupTests.cpp" />
    <ClCompile Include="DbcsTests.cpp" />
    <ClCompile Include="HistoryTests.cpp" />
    <ClCompile Include="InitTests.cpp" />
    <ClCompile Include="ObjectTests.cpp" />
    <ClCompile Include="OutputCellIteratorTests.cpp" />
    <ClCompile Include="ScreenBufferTests.cpp" />
    <ClCompile Include="SearchTests.cpp" />
    <ClCompile Include="SelectionTests.cpp" />
    <ClCompile Include="TextBufferIteratorTests.cpp" />
    <ClCompile Include="TextBufferTests.cpp" />
    <ClCompile Include="TitleTests.cpp" />
    <ClCompile Include="UtilsTests.cpp" />
    <ClCompile Include="Utf8ToWideCharParserTests.cpp" />
    <ClCompile Include="Utf16ParserTests.cpp" />
    <ClCompile Include="InputBufferTests.cpp" />
    <ClCompile Include="ReadWaitTests.cpp" />
    <ClCompile Include="ViewportTests.cpp" />
    <ClCompile Include="VtIoTests.cpp" />
    <ClCompile Include="VtRendererTests.cpp" />
    <Clcompile Include="..\..\types\IInputEventStreams.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\buffer\out\lib\bufferout.vcxproj">
      <Project>{0cf235bd-2da0-407e-90ee-c467e8bbc714}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\internal\internal.vcxproj">
      <Project>{ef3e32a7-5ff6-42b4-b6e2-96cd7d033f00}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\dx\lib\dx.vcxproj">
      <Project>{48d21369-3d7b-4431-9967-24e81292cf62}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\renderer\vt\ut_lib\vt.unittest.vcxproj">
      <Project>{990F2657-8580-4828-943F-5DD657D11843}</Project>
    </ProjectReference>
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
    <ProjectReference Include="..\ut_lib\host.unittest.vcxproj">
      <Project>{06ec74cb-9a12-429c-b551-8562ec954746}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\..\inc\CommonState.hpp" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="PopupTestHelper.hpp" />
    <ClInclude Include="UnicodeLiteral.hpp" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>..;$(SolutionDir)src\inc;$(SolutionDir)src\inc\test;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
</Project>
