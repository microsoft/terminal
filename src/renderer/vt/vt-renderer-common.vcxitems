<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!--
    Contains all the files for the VtRenderer Project.
    They need to be consumed by two different projects - lib/vt.vcxproj and
    ut_lib/vt-unittesting.vcxproj so that the tests can build a version of the
    lib with -DUNIT_TESTING
   -->
  <ItemGroup>
    <ClCompile Include="..\invalidate.cpp" />
    <ClCompile Include="..\math.cpp" />
    <ClCompile Include="..\paint.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\state.cpp" />
    <ClCompile Include="..\tracing.cpp" />
    <ClCompile Include="..\VtSequences.cpp" />
    <ClCompile Include="..\WinTelnetEngine.cpp" />
    <ClCompile Include="..\XtermEngine.cpp" />
    <ClCompile Include="..\Xterm256Engine.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\tracing.hpp" />
    <ClInclude Include="..\vtrenderer.hpp" />
    <ClInclude Include="..\WinTelnetEngine.hpp" />
    <ClInclude Include="..\XtermEngine.hpp" />
    <ClInclude Include="..\Xterm256Engine.hpp" />
  </ItemGroup>
</Project>
