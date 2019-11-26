<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{2FD12FBB-1DDB-46D8-B818-1023C624CACA}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>tsf</RootNamespace>
    <ProjectName>TextServicesFramework</ProjectName>
    <TargetName>ConTSF</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="ConsoleTSF.cpp" />
    <ClCompile Include="contsf.cpp" />
    <ClCompile Include="TfCatUtil.cpp" />
    <ClCompile Include="TfConvArea.cpp" />
    <ClCompile Include="TfDispAttr.cpp" />
    <ClCompile Include="TfEditses.cpp" />
    <ClCompile Include="TfTxtevCb.cpp" />
    <ClCompile Include="precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="ConsoleTSF.h" />
    <ClInclude Include="debug.h" />
    <ClInclude Include="globals.h" />
    <ClInclude Include="precomp.h" />
    <ClInclude Include="TfCatUtil.h" />
    <ClInclude Include="TfConvArea.h" />
    <ClInclude Include="TfCtxtComp.h" />
    <ClInclude Include="TfDispAttr.h" />
    <ClInclude Include="TfEditses.h" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
