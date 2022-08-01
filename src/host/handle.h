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

#include <til/ticket_lock.h>

struct LockConsoleGuard
{
    explicit LockConsoleGuard(std::unique_lock<til::recursive_ticket_lock>&& guard) noexcept;
    ~LockConsoleGuard();

private:
    std::unique_lock<til::recursive_ticket_lock> _guard;
};

LockConsoleGuard LockConsole();
