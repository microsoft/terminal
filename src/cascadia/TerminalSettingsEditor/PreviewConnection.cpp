// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::wstring_view PreviewText{
    L"Windows Terminal\r\n"
    L"C:\\> \x1b[93mgit\x1b[m diff \x1b[90m-w\x1b[m\r\n"
    L"\x1b[1mdiff --git a/win b/win\x1b[m\r\n"
    L"\x1b[36m@@ -1 +1 @@\x1b[m\r\n"
    L"\x1b[31m-    Windows Console\x1b[m\r\n"
    L"\x1b[32m-    Windows Terminal!\x1b[m\r\n"
    L"C:\\> \x1b[93mWrite-Host \x1b[36m\"\xd83e\xde9f!\"\x1b[1D\x1b[m"
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept = default;

    void PreviewConnection::Start() noexcept
    {
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
