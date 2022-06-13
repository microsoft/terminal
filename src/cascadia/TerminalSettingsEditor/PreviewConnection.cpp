// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PreviewConnection.h"
#include <LibraryResources.h>

using namespace ::winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    PreviewConnection::PreviewConnection(const winrt::hstring previewString) noexcept :
        _previewString{ previewString }
    {
    }

    void PreviewConnection::Start() noexcept
    {
        // First send a sequence to disable cursor blinking
        _TerminalOutputHandlers(L"\x1b[?12l");
        // Send the preview text
        _TerminalOutputHandlers(_previewString);
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
