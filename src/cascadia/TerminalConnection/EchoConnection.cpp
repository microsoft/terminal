// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EchoConnection.h"
#include <sstream>

#include "EchoConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    EchoConnection::EchoConnection() noexcept = default;

    void EchoConnection::Start() noexcept
    {
        _transitionToState(ConnectionState::Connected);
    }

    void EchoConnection::WriteInput(const winrt::array_view<const uint8_t> buffer)
    {
        const auto data = winrt_array_to_string_view(buffer);
        std::stringstream prettyPrint;
        for (const auto& ch : data)
        {
            if (ch < 0x20)
            {
                prettyPrint << "^" << gsl::narrow_cast<char>(ch + 0x40);
            }
            else if (ch == 0x7f)
            {
                prettyPrint << "0x7f";
            }
            else
            {
                prettyPrint << ch;
            }
        }
        WriteUtf8Output(prettyPrint.str());
    }

    void EchoConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void EchoConnection::Close() noexcept
    {
    }
}
