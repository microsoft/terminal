<?xml version="1.0" encoding="utf-8"?>
<Package xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities" xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" IgnorableNamespaces="uap">

  <!-- This file is used as the Appxmanifest for tests that _need_ to run in a
  packaged environment. It will be copied to the test's OutDir as part of the
  PostBuid step. It's highly similar to the "PackagedCwaFullTrust" manifest that
  TAEF ships with, with the following modifications:

  1. All of our winrt types are included in this manifest, including types from
     MUX.dll. Should this list of types ever change, we'll need to manually
     update this file. The easiest way of doing that is deploying the app from
     VS, then copying the Extensions from the Appxmanifest.xml that's generated
     under `src/cascadia/CascadiaPackage/bin/$(platform)/$(configuration)/appx`.

  2. We also _NEED_ the two vclibs listed under the `PackageDependency` block.

  If your test fails for whatever reason, it's likely possible you're testing a
  type that's _not_ included in this file for some reason. So, here be dragons. -->

  <Identity Name="TerminalApp.LocalTests.Package"
          ProcessorArchitecture="neutral"
          Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US"
          Version="1.0.0.0"
          ResourceId="en-us" />
  <Properties>
    <DisplayName>TerminalApp.LocalTests.Package Host Process</DisplayName>
    <PublisherDisplayName>Microsoft Corp.</PublisherDisplayName>
    <Logo>taef.png</Logo>
    <Description>TAEF Packaged Cwa FullTrust Application Host Process</Description>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Universal" MinVersion="10.0.18362.0" MaxVersionTested="10.0.18362.0" />
    <PackageDependency Name="Microsoft.VCLibs.140.00.Debug" MinVersion="14.0.27023.1" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
    <PackageDependency Name="Microsoft.VCLibs.140.00.Debug.UWPDesktop" MinVersion="14.0.27027.1" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="TE.ProcessHost" Executable="TE.ProcessHost.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="TAEF Packaged Cwa FullTrust Application Host Process" Square150x150Logo="taef.png" Square44x44Logo="taef.png" Description="TAEF Packaged Cwa Application Host Process" BackgroundColor="#222222">
        <uap:SplashScreen Image="taef.png" />
      </uap:VisualElements>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>



</Package>
