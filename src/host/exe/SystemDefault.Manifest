/*
This is an odd use of Sxs. See the sources file for more comments.
Conhost is created via RtlCreateUserProcess instead of CreateProcess,
so it does not end up with a system default manifest. We work around
this by having this manifest which references the system default that
we load and set as our process default at runtime.

The system default manifest is actually this:
    SYSTEM_COMPATIBLE_ASSEMBLY_VERSION_00 = \
        "L\"$(SYSTEM_COMPATIBLE_ASSEMBLY_VERSION).0.0\""
and not
    SYSTEM_COMPATIBLE_ASSEMBLY_FULL_VERSION_A

*/
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<assemblyIdentity
    name="Microsoft.Console.Host.Core"
    version=SXS_ASSEMBLY_VERSION
    processorArchitecture=SXS_PROCESSOR_ARCHITECTURE
    publicKeyToken="6595b64144ccf1df"
    type="win32"
/>
<description>Microsoft Console Host System Default</description>
<dependency>
    <dependentAssembly>
        <assemblyIdentity
            name=SYSTEM_COMPATIBLE_ASSEMBLY_NAME
            version=SYSTEM_COMPATIBLE_ASSEMBLY_VERSION_00_A
            processorArchitecture=SXS_PROCESSOR_ARCHITECTURE
            type="win32"
            publicKeyToken="6595b64144ccf1df"
        />
    </dependentAssembly>
</dependency>
</assembly>
