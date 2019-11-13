<Project Sdk="Microsoft.NET.Sdk.WindowsDesktop">

  <PropertyGroup>
    
    <TargetFrameworks>net472;netcoreapp3.0</TargetFrameworks>
    <RootNamespace>Microsoft.Terminal.Wpf</RootNamespace>
    <AssemblyName>Microsoft.Terminal.Wpf</AssemblyName>
    <UseWpf>true</UseWpf>

    <RepoBinPath>$(MSBuildThisFileDirectory)..\..\..\bin\</RepoBinPath>
    <IntermediateOutputPath>$(RepoBinPath)..\obj\$(Platform)\$(Configuration)\$(MSBuildProjectName)</IntermediateOutputPath>
    <OutputPath>$(RepoBinPath)$(Platform)\$(Configuration)\$(MSBuildProjectName)</OutputPath>
    <BeforePack>CollectNativePackContents</BeforePack>
    <GenerateDocumentationFile>true</GenerateDocumentationFile>
    <Version>0.5</Version>
  </PropertyGroup>
  
  <ItemGroup>
    <AdditionalFiles Include="$(MSBuildThisFileDirectory)stylecop.json">
      <Visible>true</Visible>
      <Link>stylecop.json</Link>
    </AdditionalFiles>
  </ItemGroup>

  <ItemGroup>
    <PackageReference Include="StyleCop.Analyzers" Version="1.1.118">
      <PrivateAssets>all</PrivateAssets>
      <IncludeAssets>runtime; build; native; contentfiles; analyzers; buildtransitive</IncludeAssets>
    </PackageReference>
  </ItemGroup>

  <Target Name="CollectNativePackContents">
    <ItemGroup>
      <None Include="$(RepoBinPath)\Win32\$(Configuration)\PublicTerminalCore.dll">
        <Pack>true</Pack>
        <PackagePath>runtimes\win-x86\native\</PackagePath>
      </None>
      <None Include="$(RepoBinPath)\x64\$(Configuration)\PublicTerminalCore.dll">
        <Pack>true</Pack>
        <PackagePath>runtimes\win-x64\native\</PackagePath>
      </None>
    </ItemGroup>
  </Target>

</Project>
