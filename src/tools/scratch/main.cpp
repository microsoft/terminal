// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <stdio.h>
#include <cstdint>

#include <TraceLoggingProvider.h>
TRACELOGGING_DECLARE_PROVIDER(g_hTerminalConnectionProvider);
// #include <telemetry/ProjectTelemetry.h>

#define TelemetryPrivacyDataTag(tag) TraceLoggingUInt64((tag), "PartA_PrivTags")

// Event privacy tags. Use the PDT macro values for the tag parameter, e.g.:
// TraceLoggingWrite(...,
//   TelemetryPrivacyDataTag(PDT_BrowsingHistory | PDT_ProductAndServiceUsage),
//   ...);
#define TelemetryPrivacyDataTag(tag) TraceLoggingUInt64((tag), "PartA_PrivTags")
#define PDT_BrowsingHistory 0x0000000000000002u
#define PDT_DeviceConnectivityAndConfiguration 0x0000000000000800u
#define PDT_InkingTypingAndSpeechUtterance 0x0000000000020000u
#define PDT_ProductAndServicePerformance 0x0000000001000000u
#define PDT_ProductAndServiceUsage 0x0000000002000000u
#define PDT_SoftwareSetupAndInventory 0x0000000080000000u

// Event categories specified via keywords, e.g.:
// TraceLoggingWrite(...,
//     TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
//     ...);
#define MICROSOFT_KEYWORD_CRITICAL_DATA 0x0000800000000000 // Bit 47
#define MICROSOFT_KEYWORD_MEASURES 0x0000400000000000 // Bit 46
#define MICROSOFT_KEYWORD_TELEMETRY 0x0000200000000000 // Bit 45
#define MICROSOFT_KEYWORD_RESERVED_44 0x0000100000000000 // Bit 44 (reserved for future assignment)

#define TraceLoggingOptionMicrosoftTelemetry() \
    TraceLoggingOptionGroup(0x4f50731a, 0x89cf, 0x4782, 0xb3, 0xe0, 0xdc, 0xe8, 0xc9, 0x4, 0x76, 0xba)

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(
    g_sudoProvider,
    "Microsoft.Windows.Sudo",
    // {6ffdd42d-46d9-5efe-68a1-3b18cb73a607}
    (0x6ffdd42d, 0x46d9, 0x5efe, 0x68, 0xa1, 0x3b, 0x18, 0xcb, 0x73, 0xa6, 0x07),
    TraceLoggingOptionMicrosoftTelemetry());
// tl:{6ffdd42d-46d9-5efe-68a1-3b18cb73a607}

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int /*argc*/, WCHAR* /*argv[]*/)
{
    TraceLoggingRegister(g_sudoProvider);
    printf("Logging some telemetry...\n");
    uint32_t mode = 12345;

    TraceLoggingWrite(
        g_sudoProvider,
        "This_Is_A_Test",
        TraceLoggingDescription("you get the picture"),
        TraceLoggingUInt32(mode),
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

    printf("  done.\n");
    TraceLoggingUnregister(g_sudoProvider);
    return 0;
}
