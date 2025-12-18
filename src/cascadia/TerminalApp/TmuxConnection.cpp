// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxConnection.h"

namespace winrt::TerminalApp::implementation
{
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
        StateChanged.raise(*this, nullptr);
    }

    winrt::guid TmuxConnection::SessionId() const noexcept
    {
        return {};
    }

    Microsoft::Terminal::TerminalConnection::ConnectionState TmuxConnection::State() const noexcept
    {
        return Microsoft::Terminal::TerminalConnection::ConnectionState::Connected;
    }

    void TmuxConnection::WriteOutput(const winrt::array_view<const char16_t> wstr)
    {
        if (!wstr.empty())
        {
            TerminalOutput.raise(wstr);
        }
    }
}
