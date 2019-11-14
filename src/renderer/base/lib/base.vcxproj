<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{AF0A096A-8B3A-4949-81EF-7DF8F0FEE91F}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>base</RootNamespace>
    <ProjectName>RendererBase</ProjectName>
    <TargetName>ConRenderBase</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="..\Cluster.cpp" />
    <ClCompile Include="..\FontInfo.cpp" />
    <ClCompile Include="..\FontInfoBase.cpp" />
    <ClCompile Include="..\FontInfoDesired.cpp" />
    <ClCompile Include="..\RenderEngineBase.cpp" />
    <ClCompile Include="..\renderer.cpp" />
    <ClCompile Include="..\thread.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\..\inc\Cluster.hpp" />
    <ClInclude Include="..\..\inc\FontInfo.hpp" />
    <ClInclude Include="..\..\inc\FontInfoBase.hpp" />
    <ClInclude Include="..\..\inc\FontInfoDesired.hpp" />
    <ClInclude Include="..\..\inc\IFontDefaultList.hpp" />
    <ClInclude Include="..\..\inc\IRenderData.hpp" />
    <ClInclude Include="..\..\inc\IRenderEngine.hpp" />
    <ClInclude Include="..\..\inc\IRenderer.hpp" />
    <ClInclude Include="..\..\inc\IRenderTarget.hpp" />
    <ClInclude Include="..\..\inc\RenderEngineBase.hpp" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\renderer.hpp" />
    <ClInclude Include="..\thread.hpp" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
