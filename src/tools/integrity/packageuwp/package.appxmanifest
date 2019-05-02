<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<Package IgnorableNamespaces="uap mp uap5 desktop4 iot2"
         xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:mp="http://schemas.microsoft.com/appx/2014/phone/manifest"
         xmlns:uap5="http://schemas.microsoft.com/appx/manifest/uap/windows10/5"
         xmlns:desktop4="http://schemas.microsoft.com/appx/manifest/desktop/windows10/4"
         xmlns:iot2="http://schemas.microsoft.com/appx/manifest/iot/windows10/2">
  <Identity Name="ConsoleIntegrityUWP" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" Version="1.1.0.0" />
  <mp:PhoneIdentity PhoneProductId="761B94F3-E09C-44A7-82A2-2284E9737922" PhonePublisherId="7BBC2B11-63F3-4C4F-AF1F-1D7D07C21E6F" />
  <Properties>
    <DisplayName>ConsoleIntegrityUWP</DisplayName>
    <PublisherDisplayName>Console Team</PublisherDisplayName>
    <Description>A console UWP application for testing console subsystem integrity level interactions</Description>
    <Logo>Assets\StoreLogo.scale-100.png</Logo>
  </Properties>
  <Applications>
    <Application Id="console"
                 Executable="conintegrityUWP.exe"
                 desktop4:Subsystem="console"
                 desktop4:SupportsMultipleInstances="true"
                 iot2:Subsystem="console"
                 iot2:SupportsMultipleInstances="true"
                 EntryPoint="windows.ConsoleApplication">
      <uap:VisualElements DisplayName="Console Integrity UWP" Description="Console Integrity UWP Test App" BackgroundColor="transparent" Square150x150Logo="Assets\Logo.png" Square44x44Logo="Assets\AppList.png">
        <uap:DefaultTile Square71x71Logo="Assets\SmallLogo.png" Square310x310Logo="Assets\LargeLogo.png" Wide310x150Logo="Assets\WideLogo.png">
          <uap:ShowNameOnTiles>
            <uap:ShowOn Tile="square150x150Logo" />
            <uap:ShowOn Tile="square310x310Logo" />
            <uap:ShowOn Tile="wide310x150Logo" />
          </uap:ShowNameOnTiles>
        </uap:DefaultTile>
        <uap:SplashScreen BackgroundColor="#767676" Image="Assets\SplashScreen.png" />
      </uap:VisualElements>
      <Extensions>
        <uap5:Extension Category="windows.appExecutionAlias">
            <uap5:AppExecutionAlias desktop4:Subsystem="console" iot2:Subsystem="console">
                <uap5:ExecutionAlias Alias="conintegrityUWP.exe"/>
            </uap5:AppExecutionAlias>
        </uap5:Extension>
      </Extensions>
    </Application>
  </Applications>
  <Capabilities>
    <uap:Capability Name="musicLibrary" />
    <Capability Name="internetClient" />
  </Capabilities>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Universal" MinVersion="10.0.17004.0" MaxVersionTested="10.0.17004.0" />
    <PackageDependency Name="Microsoft.VCLibs.140.00" MinVersion="14.0.0.0" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
  </Dependencies>
  <Resources>
    <Resource Language="en-US" />
  </Resources>
</Package>