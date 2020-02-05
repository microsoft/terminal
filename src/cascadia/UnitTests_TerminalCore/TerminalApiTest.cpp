// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
#define WCS(x) WCSHELPER(x)
#define WCSHELPER(x) L#x

    class TerminalApiTest
    {
        TEST_CLASS(TerminalApiTest);

        TEST_METHOD(SetColorTableEntry)
        {
            Terminal term;
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);

            auto settings = winrt::make<MockTermSettings>(100, 100, 100);
            term.UpdateSettings(settings);

            VERIFY_IS_TRUE(term.SetColorTableEntry(0, 100));
            VERIFY_IS_TRUE(term.SetColorTableEntry(128, 100));
            VERIFY_IS_TRUE(term.SetColorTableEntry(255, 100));

            VERIFY_IS_FALSE(term.SetColorTableEntry(256, 100));
            VERIFY_IS_FALSE(term.SetColorTableEntry(512, 100));
        }

        // Terminal::_WriteBuffer used to enter infinite loops under certain conditions.
        // This test ensures that Terminal::_WriteBuffer doesn't get stuck when
        // PrintString() is called with more code units than the buffer width.
        TEST_METHOD(PrintStringOfSurrogatePairs)
        {
            DummyRenderTarget renderTarget;
            Terminal term;
            term.Create({ 100, 100 }, 3, renderTarget);

            std::wstring text;
            text.reserve(600);

            for (size_t i = 0; i < 100; ++i)
            {
                text.append(L"𐐌𐐜𐐬");
            }

            struct Baton
            {
                HANDLE done;
                std::wstring text;
                Terminal* pTerm;
            } baton{
                CreateEventW(nullptr, TRUE, FALSE, L"done signal"),
                text,
                &term,
            };

            Log::Comment(L"Launching thread to write data.");
            const auto thread = CreateThread(
                nullptr,
                0,
                [](LPVOID data) -> DWORD {
                    const Baton& baton = *reinterpret_cast<Baton*>(data);
                    Log::Comment(L"Writing data.");
                    baton.pTerm->PrintString(baton.text);
                    Log::Comment(L"Setting event.");
                    SetEvent(baton.done);
                    return 0;
                },
                (LPVOID)&baton,
                0,
                nullptr);

            Log::Comment(L"Waiting for the write.");
            switch (WaitForSingleObject(baton.done, 2000))
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

            TerminateThread(thread, 0);
            CloseHandle(baton.done);
            return;
        }
    };
}
