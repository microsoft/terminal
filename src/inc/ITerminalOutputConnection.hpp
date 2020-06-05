// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- ITerminalOutputConnection.hpp

Abstract:
- Provides an abstraction for writing to the output pipe connected to the TTY.
    In conpty mode, this is implemented by the VtRenderer, such that other
    parts of the codebase (the state machine) can write VT sequences directly
    to the terminal controlling us.
*/

#pragma once

namespace Microsoft::Console
{
    class ITerminalOutputConnection
    {
    public:
#pragma warning(push)
#pragma warning(disable : 26432) // suppress rule of 5 violation on interface because tampering with this is fraught with peril
        virtual ~ITerminalOutputConnection() = 0;

        [[nodiscard]] virtual HRESULT WriteTerminalUtf8(const std::string_view str) = 0;
        [[nodiscard]] virtual HRESULT WriteTerminalW(const std::wstring_view wstr) = 0;
    };

    inline Microsoft::Console::ITerminalOutputConnection::~ITerminalOutputConnection() {}
#pragma warning(pop)
}
