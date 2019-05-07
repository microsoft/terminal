<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <!--
  This props file is a workaround for the fact that for wapproj projects,
  the $(SolutionDir) is never evaluated correctly. So, instead we're using this
  file to define $(OpenConsoleDir), which should be used in place of $(SolutionDir)
   -->
  <PropertyGroup Condition="'$(OpenConsoleDir)'==''">
    <OpenConsoleDir>$(MSBuildThisFileDirectory)</OpenConsoleDir>
  </PropertyGroup>

</Project>
