// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

static constexpr std::u8string_view PromptTextPlain{ u8"C:\\> " };
static constexpr std::u8string_view PromptTextPowerline{ u8"\x1b[49;34m\xe0b6\x1b[1;97;44m C:\\ \x1b[m\x1b[46;34m\xe0b8\x1b[49;36m\xe0b8\x1b[m " };

// clang-format off
static constexpr std::u8string_view PreviewText{
    u8"\x001b"
    u8"c" // Hard Reset (RIS); on separate lines to avoid becoming 0x01BC
    u8"Windows Terminal\r\n"
    u8"{0}\x1b[93m" u8"git\x1b[m diff \x1b[90m-w\x1b[m\r\n"
    u8"\x1b[1m" u8"diff --git a/win b/win\x1b[m\r\n"
    u8"\x1b[36m@@ -1 +1 @@\x1b[m\r\n"
    u8"\x1b[31m-    Windows Console\x1b[m\r\n"
    u8"\x1b[32m+    Windows Terminal!\x1b[m\r\n"
    u8"{0}\x1b[93mWrite-Host \x1b[36m\"🌯!\"\x1b[1D\x1b[m"
};
// clang-format on

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection() noexcept = default;

    void PreviewConnection::Start() noexcept
    {
        // Send the preview text
        auto e = fmt::format(PreviewText, _displayPowerlineGlyphs ? PromptTextPowerline : PromptTextPlain);
        TerminalOutput.raise(winrt_u8string_to_array_view(e));
    }

    void PreviewConnection::Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) noexcept
    {
    }

    void PreviewConnection::WriteInput(const winrt::array_view<const char16_t> /*data*/)
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
