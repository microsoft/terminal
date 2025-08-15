/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- dbcs.h

Abstract:
- Provides helpers to manage double-byte (double-width) characters for CJK languages within the text buffer
- Some items historically referred to as "FE" or "Far East" (geopol sensitive) and converted to "East Asian".
  Refers to Chinese, Japanese, and Korean languages that require significantly different handling from legacy ASCII/Latin1 characters.

Author:
- KazuM (suspected)

Revision History:
--*/

#pragma once

#include "screenInfo.hpp"

bool CheckBisectStringA(_In_reads_bytes_(cbBuf) PCHAR pchBuf, _In_ DWORD cbBuf, const CPINFO* const pCPInfo);

bool IsDBCSLeadByteConsole(const CHAR ch, const CPINFO* const pCPInfo);

BYTE CodePageToCharSet(const UINT uiCodePage);

BOOL IsAvailableEastAsianCodePage(const UINT uiCodePage);
