// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DummyConnection.h"
#include <sstream>

#include "DummyConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    DummyConnection::DummyConnection() noexcept = default;

    void DummyConnection::Start() noexcept
    {
    }

    void DummyConnection::WriteInput(const winrt::array_view<const char16_t> buffer)
    {
        const auto data = winrt_array_to_wstring_view(buffer);
        std::wstringstream prettyPrint;
        for (const auto& wch : data)
        {
            prettyPrint << wch;
        }
        TerminalInput.raise(prettyPrint.str());
    }

    void DummyConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void DummyConnection::Close() noexcept
    {
    }
}
