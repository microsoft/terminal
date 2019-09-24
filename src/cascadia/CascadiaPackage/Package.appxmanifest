<?xml version="1.0" encoding="utf-8"?>

<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:mp="http://schemas.microsoft.com/appx/2014/phone/manifest"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3"
  xmlns:uap4="http://schemas.microsoft.com/appx/manifest/uap/windows10/4"
  xmlns:uap7="http://schemas.microsoft.com/appx/manifest/uap/windows10/7"
  xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap mp rescap">

  <Identity
    Name="Microsoft.WindowsTerminal"
    Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US"
    Version="1.0.0.0" />

  <Properties>
    <DisplayName>ms-resource:AppName</DisplayName>
    <PublisherDisplayName>Microsoft Corporation</PublisherDisplayName>
    <Logo>Images\StoreLogo.png</Logo>
  </Properties>

  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.18362.0" MaxVersionTested="10.0.18362.0" />
  </Dependencies>

  <Resources>
    <Resource Language="x-generate"/>
  </Resources>

  <Applications>
    <Application Id="App"
      Executable="$targetnametoken$.exe"
      EntryPoint="$targetentrypoint$">
      <uap:VisualElements
        DisplayName="ms-resource:AppName"
        Description="ms-resource:AppDescription"
        BackgroundColor="transparent"
        Square150x150Logo="Images\Square150x150Logo.png"
        Square44x44Logo="Images\Square44x44Logo.png">
        <uap:DefaultTile
          Wide310x150Logo="Images\Wide310x150Logo.png" 
          Square71x71Logo="Images\SmallTile.png"
          Square310x310Logo="Images\LargeTile.png"
          ShortName="ms-resource:AppShortName">
          <uap:ShowNameOnTiles>
            <uap:ShowOn Tile="square150x150Logo"/>
            <uap:ShowOn Tile="wide310x150Logo"/>
            <uap:ShowOn Tile="square310x310Logo"/>
          </uap:ShowNameOnTiles>
        </uap:DefaultTile>
        <uap:SplashScreen Image="Images\SplashScreen.png"/>
      </uap:VisualElements>

      <Extensions>
        <uap3:Extension Category="windows.appExecutionAlias" Executable="WindowsTerminal.exe" EntryPoint="Windows.FullTrustApplication">
          <uap3:AppExecutionAlias>
            <desktop:ExecutionAlias Alias="wt.exe" />
          </uap3:AppExecutionAlias>
        </uap3:Extension>
      </Extensions>

    </Application>
  </Applications>

  <Capabilities>
    <Capability Name="internetClient" />
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>

  <Extensions>
    <uap7:Extension Category="windows.sharedFonts">
      <uap7:SharedFonts>
        <uap4:Font File="Cascadia.ttf" />
      </uap7:SharedFonts>
    </uap7:Extension>
  </Extensions>
</Package>
