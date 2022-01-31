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
      name="Microsoft-OneCore-Console-Host-Core.Resources"
      processorArchitecture="$(build.arch)"
      publicKeyToken="$(Build.WindowsPublicKeyToken)"
      version="$(build.version)"
      versionScope="nonSxS"
      />
  <file
      destinationPath="$(runtime.system32)\$(build.cultureNameString)\"
      importPath="$(build.locsrc)\"
      name="Conhost.exe.mui"
      sourceName="Conhost.exe.mui"
      sourcePath=".\"
      />
  <localization>
    <resources culture="en-US">
      <stringTable>
        <string
            id="displayName"
            value="Console Host resources (English)"
            />
        <string
            id="description"
            value="Windows Console Host MUI resources (English)"
            />
      </stringTable>
    </resources>
  </localization>
</assembly>
