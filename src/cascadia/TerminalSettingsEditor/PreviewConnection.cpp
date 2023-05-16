// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::wstring_view PreviewText{
    L"Windows Terminal\r\n"
    L"C:\\> \e[93mgit\e[m diff\r\n"
    L"\e[1mdiff --git a/hi b/hi\e[m\r\n"
    L"\e[36m@@ -0,1 +0,1 @@\e[m\r\n"
    L"\e[31m-    Windows Console\e[m\r\n"
    L"\e[32m-    Windows Terminal!\e[m\r\n"
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
