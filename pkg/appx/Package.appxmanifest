<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:mp="http://schemas.microsoft.com/appx/2014/phone/manifest" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities" xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3" xmlns:uap5="http://schemas.microsoft.com/appx/manifest/uap/windows10/5" xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10" xmlns:desktop4="http://schemas.microsoft.com/appx/manifest/desktop/windows10/4" IgnorableNamespaces="uap mp rescap">
  <Identity Name="Microsoft.WindowsConsoleHost" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" Version="0.9.0.0" />
  <Properties>
    <DisplayName>Windows Console (Preview)</DisplayName>
    <PublisherDisplayName>Microsoft Corporation</PublisherDisplayName>
    <Logo>images\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.14393.0" MaxVersionTested="10.0.14393.0" />
  </Dependencies>
  <Resources>
    <Resource Language="x-generate" />
  </Resources>
  <Applications>
    <Application Id="App" Executable="OpenConsole.exe" EntryPoint="$targetentrypoint$">
      <Extensions>
        <uap3:Extension Category="windows.appExecutionAlias" EntryPoint="Windows.FullTrustApplication" Executable="OpenConsole.exe">
          <uap3:AppExecutionAlias>
            <desktop:ExecutionAlias Alias="OpenConsole.exe" />
            <desktop:ExecutionAlias Alias="confans.exe" />
          </uap3:AppExecutionAlias>
        </uap3:Extension>
      </Extensions>
      <uap:VisualElements DisplayName="Windows Console (Preview)" Description="The Windows Console, but actually fun!" BackgroundColor="transparent" Square150x150Logo="images\Square150x150Logo.png" Square44x44Logo="images\Square44x44Logo.png">
        <uap:DefaultTile Wide310x150Logo="images\Wide310x150Logo.png">
        </uap:DefaultTile>
      </uap:VisualElements>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
</Package>
