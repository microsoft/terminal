<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="15.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <Import Project="$(WapProjPath)\Microsoft.DesktopBridge.targets" />
  <ItemGroup>
    <SDKReference Include="Microsoft.VCLibs,Version=14.0">
      <TargetedSDKConfiguration Condition="'$(Configuration)'!='Debug'">Retail</TargetedSDKConfiguration>
      <TargetedSDKConfiguration Condition="'$(Configuration)'=='Debug'">Debug</TargetedSDKConfiguration>
      <TargetedSDKArchitecture>$(PlatformShortName)</TargetedSDKArchitecture>
      <Implicit>true</Implicit>
    </SDKReference>
  </ItemGroup>

</Project>
