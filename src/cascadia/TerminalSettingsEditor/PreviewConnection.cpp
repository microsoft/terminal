// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::wstring_view PreviewText{
    L"Windows Terminal\r\n"
    L"C:\\> \x1b[93mgit\x1b[m diff\r\n"
    L"\x1b[1mdiff --git a/hi b/hi\x1b[m\r\n"
    L"\x1b[36m@@ -0,1 +0,1 @@\x1b[m\r\n"
    L"\x1b[31m-    Windows Console\x1b[m\r\n"
    L"\x1b[32m-    Windows Terminal!\x1b[m\r\n"
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept = default;

    void PreviewConnection::Start() noexcept
    {
        // First send a sequence to disable cursor blinking
        _TerminalOutputHandlers(L"\x1b[?12l");
        // Send the preview text
        _TerminalOutputHandlers(PreviewText);
    }

    void PreviewConnection::Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) noexcept
    {
    }

    void PreviewConnection::WriteInput(const hstring& /*data*/)
    {
    }

    void PreviewConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void PreviewConnection::Close() noexcept
    {
    }
}
