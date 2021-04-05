// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;
namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept
    {
    }

    void PreviewConnection::Start() noexcept
    {
        _TerminalOutputHandlers(fmt::format(L"{}\r\n\x1b[31m{}\r\n\x1b[32m{}\r\n\x1b[34m{}",
                                RS_(L"PreviewConnection_DefaultText"),
                                RS_(L"PreviewConnection_RedText"),
                                RS_(L"PreviewConnection_GreenText"),
                                RS_(L"PreviewConnection_BlueText")));
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
