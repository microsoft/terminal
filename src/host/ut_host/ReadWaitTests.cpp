// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "misc.h"
#include "dbcs.h"
#include "../../types/inc/IInputEvent.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

#include <deque>
#include <memory>

using namespace WEX::Logging;
using Microsoft::Console::Interactivity::ServiceLocator;

class InputRecordConversionTests
{
    TEST_CLASS(InputRecordConversionTests);

    static const size_t INPUT_RECORD_COUNT = 10;
    UINT savedCodepage = 0;

    TEST_CLASS_SETUP(ClassSetup)
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        savedCodepage = gci.CP;
        gci.CP = CP_JAPANESE;
        VERIFY_IS_TRUE(!!GetCPInfo(gci.CP, &gci.CPInfo));
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.CP = savedCodepage;
        VERIFY_IS_TRUE(!!GetCPInfo(gci.CP, &gci.CPInfo));
        return true;
    }

    TEST_METHOD(SplitToOemLeavesNonKeyEventsAlone)
    {
        Log::Comment(L"nothing should happen to input events that aren't key events");

        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        INPUT_RECORD inRecords[INPUT_RECORD_COUNT] = { 0 };
        for (size_t i = 0; i < INPUT_RECORD_COUNT; ++i)
        {
            inRecords[i].EventType = MOUSE_EVENT;
            inRecords[i].Event.MouseEvent.dwMousePosition.X = static_cast<SHORT>(i);
            inRecords[i].Event.MouseEvent.dwMousePosition.Y = static_cast<SHORT>(i * 2);
            inEvents.push_back(IInputEvent::Create(inRecords[i]));
        }

        SplitToOem(inEvents);
        VERIFY_ARE_EQUAL(INPUT_RECORD_COUNT, inEvents.size());

        for (size_t i = 0; i < INPUT_RECORD_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(inRecords[i], inEvents[i]->ToInputRecord());
        }
    }

    TEST_METHOD(SplitToOemLeavesNonDbcsCharsAlone)
    {
        Log::Comment(L"non-dbcs chars shouldn't be split");

        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        INPUT_RECORD inRecords[INPUT_RECORD_COUNT] = { 0 };
        for (size_t i = 0; i < INPUT_RECORD_COUNT; ++i)
        {
            inRecords[i].EventType = KEY_EVENT;
            inRecords[i].Event.KeyEvent.uChar.UnicodeChar = static_cast<wchar_t>(L'a' + i);
            inEvents.push_back(IInputEvent::Create(inRecords[i]));
        }

        SplitToOem(inEvents);
        VERIFY_ARE_EQUAL(INPUT_RECORD_COUNT, inEvents.size());

        for (size_t i = 0; i < INPUT_RECORD_COUNT; ++i)
        {
            VERIFY_ARE_EQUAL(inRecords[i], inEvents[i]->ToInputRecord());
        }
    }

    TEST_METHOD(SplitToOemSplitsDbcsChars)
    {
        Log::Comment(L"dbcs chars should be split");

        const UINT codepage = ServiceLocator::LocateGlobals().getConsoleInformation().CP;

        INPUT_RECORD inRecords[INPUT_RECORD_COUNT * 2] = { 0 };
        std::deque<std::unique_ptr<IInputEvent>> inEvents;
        // U+3042 hiragana letter A
        wchar_t hiraganaA = 0x3042;
        wchar_t inChars[INPUT_RECORD_COUNT];
        for (size_t i = 0; i < INPUT_RECORD_COUNT; ++i)
        {
            wchar_t currentChar = static_cast<wchar_t>(hiraganaA + (i * 2));
            inRecords[i].EventType = KEY_EVENT;
            inRecords[i].Event.KeyEvent.uChar.UnicodeChar = currentChar;
            inChars[i] = currentChar;
            inEvents.push_back(IInputEvent::Create(inRecords[i]));
        }

        SplitToOem(inEvents);
        VERIFY_ARE_EQUAL(INPUT_RECORD_COUNT * 2, inEvents.size());

        // create the data to compare the output to
        char dbcsChars[INPUT_RECORD_COUNT * 2] = { 0 };
        int writtenBytes = WideCharToMultiByte(codepage,
                                               0,
                                               inChars,
                                               INPUT_RECORD_COUNT,
                                               dbcsChars,
                                               INPUT_RECORD_COUNT * 2,
                                               nullptr,
                                               false);
        VERIFY_ARE_EQUAL(writtenBytes, static_cast<int>(INPUT_RECORD_COUNT * 2));
        for (size_t i = 0; i < INPUT_RECORD_COUNT * 2; ++i)
        {
            const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(inEvents[i].get());
            VERIFY_ARE_EQUAL(static_cast<char>(pKeyEvent->GetCharData()), dbcsChars[i]);
        }
    }
};
