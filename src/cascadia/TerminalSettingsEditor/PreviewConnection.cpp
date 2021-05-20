// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::wstring_view PreviewText{ L"Windows Terminal\r\nCopyright (c) Microsoft Corporation\r\n\nC:\\Windows\\Terminal> " };

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept
    {
    }

    void PreviewConnection::Start() noexcept
    {
        // First send a sequence to disable cursor blinking
        _TerminalOutputHandlers(L"\x1b[?12l");
        // Send the preview text
        _TerminalOutputHandlers(PreviewText);
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
