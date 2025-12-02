// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxConnection.h"
#include <sstream>

#include "TmuxConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    TmuxConnection::TmuxConnection() noexcept = default;

    void TmuxConnection::Initialize(const Windows::Foundation::Collections::ValueSet&) const noexcept
    {
    }

    void TmuxConnection::Start() noexcept
    {
    }

    void TmuxConnection::WriteInput(const winrt::array_view<const char16_t> buffer)
    {
        TerminalInput.raise(buffer);
    }

    void TmuxConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void TmuxConnection::Close() noexcept
    {
    }

    winrt::guid TmuxConnection::SessionId() const noexcept
    {
        return {};
    }

    ConnectionState TmuxConnection::State() const noexcept
    {
        return ConnectionState::Connected;
    }

    void TmuxConnection::WriteOutput(const winrt::array_view<const char16_t> wstr)
    {
        if (!wstr.empty())
        {
            TerminalOutput.raise(wstr);
        }
    }
}
