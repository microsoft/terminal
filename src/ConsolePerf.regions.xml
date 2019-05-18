<?xml version="1.0" encoding="utf-8"?>
<InstrumentationManifest xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <Instrumentation>
        <Regions>
        <!-- The RegionRoot and Region GUID are made up. Make some up. -->
            <RegionRoot Guid="{ff1020e3-3f3a-3656-44a0-096b0b9b575a}" Name="TAEF">
                <Region Guid="{e1f18b03-d49c-6f10-3753-2dc36fa663f1}" Name="TestRun">
                    <Start>
                        <!-- This GUID corresponds to one of the TAEF providers that will send this start/stop event pair. -->
                        <Event Provider="{70d27130-f2f3-4365-b790-d31223254ef4}" Name="RunTest_TestRunScope" Opcode="1"/>
                    </Start>
                    <Stop>
                        <Event Provider="{70d27130-f2f3-4365-b790-d31223254ef4}" Name="RunTest_TestRunScope" Opcode="2"/>
                    </Stop>
                    <Match>
                        <Event>
                            <Payload FieldName="ScopeName"/>
                        </Event>
                    </Match>
                    <Naming>
                        <PayloadBased NameField="ScopeName"/>
                    </Naming>
                    <Metadata>
                        <!-- At a bare minimum, you must collect something to get the wall time measurement. -->
                        <!-- The WinPerf team recommended collecting CPU if you don't care about anything else. -->
                        <WinperfWPAPreset.1>CPU</WinperfWPAPreset.1>
                        <WinperfWPAPreset.1.ProcessName>te.processhost.exe</WinperfWPAPreset.1.ProcessName>
                    </Metadata>
                </Region>
                <!-- Again, this GUID is just straight up made up. -->
                <!-- Also, just keep making regions. There's also a provision to stack a region inside a region. -->
                <!-- Go look up "Region of Interest" files in respect to WPA to learn more. -->
                <Region Guid="{A7E0E48F-F95D-49A9-83F5-560E8A0279FB}" Name="ApiCall">
                    <Start>
                        <!-- This GUID corresponds to Console Host provider which will send the start/stop pair. -->
                        <Event Provider="{fe1ff234-1f09-50a8-d38d-c44fab43e818}" Name="ApiCall" Opcode="1"/>
                    </Start>
                    <Stop>
                        <Event Provider="{fe1ff234-1f09-50a8-d38d-c44fab43e818}" Name="ApiCall" Opcode="2"/>
                    </Stop>
                    <Match>
                        <Event>
                            <Payload FieldName="ApiName"/>
                        </Event>
                    </Match>
                    <Naming>
                        <PayloadBased NameField="ApiName"/>
                    </Naming>
                    <Metadata>
                        <WinperfWPAPreset.1>CPU</WinperfWPAPreset.1>
                        <!-- Semicolon delimiting the process names (or any other field) is OK here. -->
                        <WinperfWPAPreset.1.ProcessName>conhost.exe;openconsole.exe</WinperfWPAPreset.1.ProcessName>
                        <!-- Just keep incrementing the numbers. See the OSGWiki on WinPerf for the valid preset names. -->
                        <!-- https://osgwiki.com/wiki/Winperf#Leveraging_Built-In_Resource_Utilization_Metrics -->
                        <!-- DO NOTE: Using a preset here isn't the end of it. This is just specifying that we're interested in it. -->
                        <!-- You must also go change the .wprp to COLLECT the relevant data in the first place. --> 
                        <WinperfWPAPreset.2>Commit</WinperfWPAPreset.2>
                        <WinperfWPAPreset.2.ProcessName>conhost.exe;openconsole.exe</WinperfWPAPreset.2.ProcessName>
                    </Metadata>
                </Region>
            </RegionRoot>
        </Regions>
    </Instrumentation>
</InstrumentationManifest>
