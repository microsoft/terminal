/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleInputThread.hpp

Abstract:
- Win32 implementation of the IConsoleInputThread interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#include "precomp.h"

#include "..\inc\IConsoleInputThread.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class ConsoleInputThread final : public IConsoleInputThread
    {
    public:
        ~ConsoleInputThread() = default;
        HANDLE Start();
    };
}
