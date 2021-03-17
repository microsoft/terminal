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
    }

    void PreviewConnection::WriteInput(hstring const& data)
    {
        std::wstringstream prettyPrint;
        for (const auto& wch : data)
        {
            if (wch < 0x20)
            {
                prettyPrint << L"^" << gsl::narrow_cast<wchar_t>(wch + 0x40);
            }
            else if (wch == 0x7f)
            {
                prettyPrint << L"0x7f";
            }
            else
            {
                prettyPrint << wch;
            }
        }
        _TerminalOutputHandlers(prettyPrint.str());
    }

    void PreviewConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void PreviewConnection::Close() noexcept
    {
    }
}
