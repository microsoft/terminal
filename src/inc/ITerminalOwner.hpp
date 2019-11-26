/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- ITerminalOwner.hpp

Abstract:
- Provides an abstraction for Closing the Input/Output objects of a terminal
    connection. This is implemented by VtIo in the host, and is used by the
    renderer to be able to tell the VtIo object that the renderer has had it's
    pipe broken.

Author(s):
- Mike Griese (migrie) 28 March 2018
--*/

#pragma once

namespace Microsoft::Console
{
    class ITerminalOwner
    {
    public:
        virtual ~ITerminalOwner() = 0;

        virtual void CloseInput() = 0;
        virtual void CloseOutput() = 0;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline Microsoft::Console::ITerminalOwner::~ITerminalOwner() {}
}
