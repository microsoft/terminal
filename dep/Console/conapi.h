/*++
Copyright (c) Microsoft Corporation

Module Name:
- conapi.h

Abstract:
- This module contains the internal structures and definitions used by the console server.

Author:
- Therese Stowell (ThereseS) 12-Nov-1990

Revision History:
--*/

#pragma once
// these should be in precomp but aren't being picked up...
#include <unordered_map>
#define STATUS_SHARING_VIOLATION         ((NTSTATUS)0xC0000043L)

#include "conmsgl1.h"
#include "conmsgl2.h"
#include "conmsgl3.h"

