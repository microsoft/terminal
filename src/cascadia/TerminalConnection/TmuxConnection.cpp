// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxConnection.h"
#include <sstream>

#include "TmuxConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    TmuxConnection::TmuxConnection() noexcept = default;

    void TmuxConnection::Start() noexcept
    {
    }

    void TmuxConnection::WriteInput(const winrt::array_view<const char16_t> buffer)
    {
        const auto data = winrt_array_to_wstring_view(buffer);
        std::wstringstream prettyPrint;
        for (const auto& wch : data)
        {
            prettyPrint << wch;
        }
        TerminalInput.raise(prettyPrint.str());
    }

    void TmuxConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void TmuxConnection::Close() noexcept
    {
    }
}
