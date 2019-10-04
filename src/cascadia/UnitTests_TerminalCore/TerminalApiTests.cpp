/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*/
#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../cascadia/UnitTests_TerminalCore/MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace winrt::Microsoft::Terminal::Settings;

namespace TerminalCoreUnitTests
{
    class TerminalApiTests
    {
        TEST_CLASS(TerminalApiTests);

        struct Baton
        {
            HANDLE ev;
            std::wstring text;
            Terminal* pTerm;
        };

        TEST_METHOD(PrintStringOfEmojiBisectingFinalColumn)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            std::wstring textToPrint;
            textToPrint.push_back(L'A'); // A is half-width, push it in.

            // Put a ton of copies of a full-width emoji here.
            const wchar_t* emoji = L"\xD83D\xDE00"; // 1F600 is wide in https://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
            for (size_t i = 0; i < 120; ++i)
            {
                textToPrint = textToPrint.append(emoji);
            }

            Baton b;
            b.ev = CreateEventW(nullptr, TRUE, FALSE, L"It is an event");
            b.text = textToPrint;
            b.pTerm = &term;

            Log::Comment(L"Launching thread to write data.");

            HANDLE hThread = CreateThread(
                nullptr, 0, [](LPVOID c) -> DWORD {
                    Baton& b = *reinterpret_cast<Baton*>(c);
                    Log::Comment(L"Writing data.");
                    b.pTerm->PrintString(b.text);
                    Log::Comment(L"Setting event.");
                    SetEvent(b.ev);
                    return 0;
                },
                (LPVOID)&b,
                0,
                nullptr);

            Log::Comment(L"Waiting for the write.");
            switch (WaitForSingleObject(b.ev, 2000))
            {
            case WAIT_OBJECT_0:
                Log::Comment(L"Didn't get stuck. Success.");
                break;
            case WAIT_TIMEOUT:
                Log::Comment(L"Wait timed out. It got stuck.");
                Log::Result(WEX::Logging::TestResults::Failed);
                break;
            case WAIT_FAILED:
                Log::Comment(L"Wait failed for some reason. We didn't expect this.");
                Log::Result(WEX::Logging::TestResults::Failed);
                break;
            default:
                Log::Comment(L"Wait return code that no one expected. Fail.");
                Log::Result(WEX::Logging::TestResults::Failed);
                break;
            }

            TerminateThread(hThread, 0);
            return;
        }
    };
}
