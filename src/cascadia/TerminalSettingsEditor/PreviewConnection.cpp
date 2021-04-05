// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;
namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept
    {
    }

    void PreviewConnection::Start() noexcept
    {
        _TerminalOutputHandlers(L"this needs to be updated! use ansii sequences to send color");
    }

    void PreviewConnection::WriteInput(hstring const& /*data*/)
    {
    }

    void PreviewConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void PreviewConnection::Close() noexcept
    {
    }
}
