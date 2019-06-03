/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleInputThread.hpp

Abstract:
- OneCore implementation of the IConsoleInputThread interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "..\inc\IConsoleInputThread.hpp"
#include "ConIoSrvComm.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class ConsoleInputThread sealed : public IConsoleInputThread
    {
    public:
        HANDLE Start();

        ConIoSrvComm* GetConIoSrvComm();

    private:
        ConIoSrvComm* _pConIoSrvComm;
    };
}
