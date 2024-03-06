// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::wstring_view PromptTextPlain{ L"C:\\> " };
static constexpr std::wstring_view PromptTextPowerline{ L"\x1b[49;34m\xe0b6\x1b[1;97;44m C:\\ \x1b[m\x1b[46;34m\xe0b8\x1b[49;36m\xe0b8\x1b[m " };

// clang-format off
static constexpr std::wstring_view PreviewText{
    L"\x001b"
    L"c" // Hard Reset (RIS); on separate lines to avoid becoming 0x01BC
    L"Windows Terminal\r\n"
    L"{0}\x1b[93m" L"git\x1b[m diff \x1b[90m-w\x1b[m\r\n"
    L"\x1b[1m" L"diff --git a/win b/win\x1b[m\r\n"
    L"\x1b[36m@@ -1 +1 @@\x1b[m\r\n"
    L"\x1b[31m-    Windows Console\x1b[m\r\n"
    L"\x1b[32m+    Windows Terminal!\x1b[m\r\n"
    L"{0}\x1b[93mWrite-Host \x1b[36m\"\xd83c\xdf2f!\"\x1b[1D\x1b[m"
};
// clang-format on

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept = default;

    void PreviewConnection::Start() noexcept
    {
        // Send the preview text
        _TerminalOutputHandlers(fmt::format(PreviewText, _displayPowerlineGlyphs ? PromptTextPowerline : PromptTextPlain));
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

    void PreviewConnection::DisplayPowerlineGlyphs(bool d) noexcept
    {
        if (_displayPowerlineGlyphs != d)
        {
            _displayPowerlineGlyphs = d;
            Start();
        }
    }
}
