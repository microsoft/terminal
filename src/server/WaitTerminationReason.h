/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WaitTerminationReason.h

Abstract:
- This file defines the reasons to terminate a wait block

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in input.h
--*/

#pragma once

enum class WaitTerminationReason
{
    NoReason = 0x0,
    CtrlC = 0x1,
    CtrlBreak = 0x2,
    ThreadDying = 0x4,
    HandleClosing = 0x8
};

DEFINE_ENUM_FLAG_OPERATORS(WaitTerminationReason);
