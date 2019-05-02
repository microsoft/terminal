/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- handle.h

Abstract:
- This file manages console and I/O handles.
- Mainly related to process management/interprocess communication.

Author:
- Therese Stowell (ThereseS) 16-Nov-1990

Revision History:
--*/

#pragma once

void LockConsole();
void UnlockConsole();
