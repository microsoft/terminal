<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProjectGuid>{18D09A24-8240-42D6-8CB6-236EEE820262}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>server</RootNamespace>
    <ProjectName>Server</ProjectName>
    <TargetName>ConServer</TargetName>
    <ConfigurationType>StaticLibrary</ConfigurationType>
  </PropertyGroup>
  <Import Project="$(SolutionDir)src\common.build.pre.props" />
  <ItemGroup>
    <ClCompile Include="..\ApiDispatchers.cpp" />
    <ClCompile Include="..\ApiDispatchersInternal.cpp" />
    <ClCompile Include="..\ApiMessage.cpp" />
    <ClCompile Include="..\ApiMessageState.cpp" />
    <ClCompile Include="..\ApiSorter.cpp" />
    <ClCompile Include="..\DeviceComm.cpp" />
    <ClCompile Include="..\DeviceHandle.cpp" />
    <ClCompile Include="..\Entrypoints.cpp" />
    <ClCompile Include="..\IoDispatchers.cpp" />
    <ClCompile Include="..\IoSorter.cpp" />
    <ClCompile Include="..\ObjectHandle.cpp" />
    <ClCompile Include="..\ObjectHeader.cpp" />
    <ClCompile Include="..\precomp.cpp">
      <PrecompiledHeader>Create</PrecompiledHeader>
    </ClCompile>
    <ClCompile Include="..\ProcessHandle.cpp" />
    <ClCompile Include="..\ProcessList.cpp" />
    <ClCompile Include="..\ProcessPolicy.cpp" />
    <ClCompile Include="..\WaitBlock.cpp" />
    <ClCompile Include="..\WaitQueue.cpp" />
    <ClCompile Include="..\WinNTControl.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="..\ApiDispatchers.h" />
    <ClInclude Include="..\ApiMessage.h" />
    <ClInclude Include="..\ApiMessageState.h" />
    <ClInclude Include="..\ApiSorter.h" />
    <ClInclude Include="..\DeviceComm.h" />
    <ClInclude Include="..\DeviceHandle.h" />
    <ClInclude Include="..\Entrypoints.h" />
    <ClInclude Include="..\IApiRoutines.h" />
    <ClInclude Include="..\IoDispatchers.h" />
    <ClInclude Include="..\IoSorter.h" />
    <ClInclude Include="..\IWaitRoutine.h" />
    <ClInclude Include="..\ObjectHandle.h" />
    <ClInclude Include="..\ObjectHeader.h" />
    <ClInclude Include="..\precomp.h" />
    <ClInclude Include="..\ProcessHandle.h" />
    <ClInclude Include="..\ProcessList.h" />
    <ClInclude Include="..\ProcessPolicy.h" />
    <ClInclude Include="..\WaitBlock.h" />
    <ClInclude Include="..\WaitQueue.h" />
    <ClInclude Include="..\WaitTerminationReason.h" />
    <ClInclude Include="..\WinNTControl.h" />
  </ItemGroup>
  <!-- Careful reordering these. Some default props (contained in these files) are order sensitive. -->
  <Import Project="$(SolutionDir)src\common.build.post.props" />
</Project>
