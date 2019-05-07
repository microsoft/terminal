/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ProjectTelemetry.h

Abstract:
- This module is used for basic definitions for telemetry for the entire project
--*/

#define TraceLoggingOptionMicrosoftTelemetry() TraceLoggingOptionGroup(0x9aa7a361, 0x583f, 0x4c09, 0xb1, 0xf1, 0xce, 0xa1, 0xef, 0x58, 0x63, 0xb0)
#define TelemetryPrivacyDataTag(tag) TraceLoggingUInt64((tag), "PartA_PrivTags")
#define PDT_ProductAndServicePerformance       0x0u
#define PDT_ProductAndServiceUsage             0x0u
#define MICROSOFT_KEYWORD_TELEMETRY     0x0
#define MICROSOFT_KEYWORD_MEASURES      0x0