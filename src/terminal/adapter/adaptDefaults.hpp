/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- adaptDefaults.hpp

Abstract:
- This serves as an abstraction for the default cases in the state machine (which is to just print or execute a simple single character.
- This can also handle processing of an entire string of printable characters, as an optimization.
- When using the Windows Console API adapter (AdaptDispatch), this must be passed in to signify where standard actions should go.

Author(s):
- Michael Niksa (MiNiksa) 30-July-2015
- Mike Griese (migrie) 07-March-2016
--*/
#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDefaults
    {
    public:
        virtual void Print(const wchar_t wch) = 0;
        // These characters need to be mutable so that they can be processed by the TerminalInput translater.
        virtual void PrintString(const wchar_t* const rgwch, const size_t cch) = 0;
        virtual void Execute(const wchar_t wch) = 0;
    };
}
