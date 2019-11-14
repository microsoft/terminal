// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EchoConnection.h"
#include <sstream>

#include "EchoConnection.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    EchoConnection::EchoConnection()
    {
    }

    void EchoConnection::Start()
    {
    }

    void EchoConnection::WriteInput(hstring const& data)
    {
        std::wstringstream prettyPrint;
        for (wchar_t wch : data)
        {
            if (wch < 0x20)
            {
                prettyPrint << L"^" << (wchar_t)(wch + 0x40);
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

    void EchoConnection::Resize(uint32_t rows, uint32_t columns)
    {
        rows;
        columns;

        throw hresult_not_implemented();
    }

    void EchoConnection::Close()
    {
        throw hresult_not_implemented();
    }
}
