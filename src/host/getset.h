/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- getset.h

Abstract:
- This file implements the NT console server console state API.

Author:
- Therese Stowell (ThereseS) 5-Dec-1990

Revision History:
--*/

#pragma once
#include "../inc/conattrs.hpp"
class SCREEN_INFORMATION;

[[nodiscard]] HRESULT DoSrvSetConsoleOutputCodePage(const unsigned int codepage);
