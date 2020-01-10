// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EchoConnection.h"
#include <sstream>

// We have to define GSL here, not PCH
// because TelnetConnection has a conflicting GSL implementation.
#include <gsl/gsl>

#include "EchoConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    EchoConnection::EchoConnection() noexcept
    {
    }

    void EchoConnection::Start() noexcept
    {
    }

    void EchoConnection::WriteInput(hstring const& data)
    {
        std::wstringstream prettyPrint;
        for (const auto& wch : data)
        {
            if (wch < 0x20)
            {
                prettyPrint << L"^" << gsl::narrow_cast<wchar_t>(wch + 0x40);
            }
            else if (wch == 0x7f)
            {
                prettyPrint << L"0x7f";
            }
            else
            {
                prettyPrint << wch;
            }
        }
        _TerminalOutputHandlers(prettyPrint.str());
    }

    void EchoConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void EchoConnection::Close() noexcept
    {
    }
}
