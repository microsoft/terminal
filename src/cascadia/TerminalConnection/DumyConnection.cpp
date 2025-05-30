// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DumyConnection.h"
#include <sstream>

#include "DumyConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    DumyConnection::DumyConnection() noexcept = default;

    void DumyConnection::Start() noexcept
    {
    }

    void DumyConnection::WriteInput(const winrt::array_view<const char16_t> /*buffer*/)
    {
    }

    void DumyConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void DumyConnection::Close() noexcept
    {
    }
}
