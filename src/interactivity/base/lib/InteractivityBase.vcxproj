<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{06EC74CB-9A12-429C-B551-8562EC964846}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>Base</RootNamespace>
    <ProjectName>InteractivityBase</ProjectName>
    <TargetName>ConInteractivityBaseLib</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <Link>
      <AdditionalDependencies />
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <Link>
      <AdditionalDependencies />
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="..\ApiDetector.cpp" />
    <ClCompile Include="..\InteractivityFactory.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\ServiceLocator.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\..\inc\IAccessibilityNotifier.hpp" />
    <ClInclude Include="..\..\inc\IConsoleControl.hpp" />
    <ClInclude Include="..\..\inc\IConsoleInputThread.hpp" />
    <ClInclude Include="..\..\inc\IHighDpiApi.hpp" />
    <ClInclude Include="..\..\inc\IInputServices.hpp" />
    <ClInclude Include="..\..\inc\IInteractivityFactory.hpp" />
    <ClInclude Include="..\..\inc\ISystemConfigurationProvider.hpp" />
    <ClInclude Include="..\..\inc\IWindowMetrics.hpp" />
    <ClInclude Include="..\..\inc\Module.hpp" />
    <ClInclude Include="..\..\inc\VtApiRedirection.hpp" />
    <ClInclude Include="..\ApiDetector.hpp" />
    <ClInclude Include="..\InteractivityFactory.hpp" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\..\inc\ServiceLocator.hpp" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
