<?xml version='1.0' encoding='utf-8' standalone='yes'?>
<assembly
    xmlns="urn:schemas-microsoft-com:asm.v3"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    manifestVersion="1.0"
    >
  <assemblyIdentity
      buildType="$(build.buildType)"
      language="en-us"
      name="Microsoft-OneCore-Console-Host-PropSheet.Resources"
      processorArchitecture="$(build.arch)"
      publicKeyToken="$(Build.WindowsPublicKeyToken)"
      version="$(build.version)"
      versionScope="nonSxS"
      />
  <file
      buildFilter="not build.isWow"
      destinationPath="$(runtime.system32)\$(build.cultureNameString)"
      importPath="$(build.locsrc)\"
      name="console.dll.mui"
      sourceName="console.dll.mui"
      sourcePath=".\"
      />
  <localization>
    <resources culture="en-US">
      <stringTable>
        <string
            id="displayName"
            value="console.dll.mui"
            />
        <string
            id="description"
            value="Manifest for console.dll.mui"
            />
      </stringTable>
    </resources>
  </localization>
</assembly>
