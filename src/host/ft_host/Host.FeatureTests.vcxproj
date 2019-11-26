<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{8CDB8850-7484-4EC7-B45B-181F85B2EE54}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>HostFeatureTests</RootNamespace>
    <ProjectName>Host.Tests.Feature</ProjectName>
    <TargetName>ConHost.Feature.Tests</TargetName>
    <ConfigurationType>DynamicLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="API_AliasTests.cpp" />
    <ClCompile Include="API_BufferTests.cpp" />
    <ClCompile Include="API_CursorTests.cpp" />
    <ClCompile Include="API_DimensionsTests.cpp" />
    <ClCompile Include="API_FileTests.cpp" />
    <ClCompile Include="API_FillOutputTests.cpp" />
    <ClCompile Include="API_FontTests.cpp" />
    <ClCompile Include="API_InputTests.cpp" />
    <ClCompile Include="API_ModeTests.cpp" />
    <ClCompile Include="API_OutputTests.cpp" />
    <ClCompile Include="API_RgbColorTests.cpp" />
    <ClCompile Include="API_TitleTests.cpp" />
    <ClCompile Include="API_PolicyTests.cpp" />
    <ClCompile Include="CanaryTests.cpp" />
    <ClCompile Include="CJK_DbcsTests.cpp" />
    <ClCompile Include="Common.cpp" />
    <ClCompile Include="InitTests.cpp" />
    <ClCompile Include="Message_KeyPressTests.cpp" />
    <ClCompile Include="OneCoreDelay.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="API_AliasTestsHelpers.hpp" />
    <ClInclude Include="Common.hpp" />
    <ClInclude Include="OneCoreDelay.hpp" />
    <ClInclude Include="precomp.h" />
    <ClInclude Include="resource.h" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\types\lib\types.vcxproj">
      <Project>{18d09a24-8240-42d6-8cb6-236eee820263}</Project>
    </ProjectReference>
    <ProjectReference Include="..\..\winconpty\winconpty.vcxproj">
      <Project>{58a03bb2-df5a-4b66-91a0-7ef3ba01269a}</Project>
    </ProjectReference>
  </ItemGroup>
  <ItemGroup>
    <ResourceCompile Include="Host.Tests.Feature.rc" />
  </ItemGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
  </ItemDefinitionGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
  <Import Project="$(SolutionDir)src\common.build.tests.props" />
  <ItemDefinitionGroup>
    <Link>
      <AdditionalDependencies>$(OutDir)\conpty.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
</Project>