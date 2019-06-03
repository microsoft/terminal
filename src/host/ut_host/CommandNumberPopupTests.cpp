// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"
#include "PopupTestHelper.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../CommandNumberPopup.hpp"
#include "../CommandListPopup.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

static constexpr size_t BUFFER_SIZE = 256;

class CommandNumberPopupTests
{
    TEST_CLASS(CommandNumberPopupTests);

    std::unique_ptr<CommonState> m_state;
    CommandHistory* m_pHistory;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = std::make_unique<CommonState>();
        m_state->PrepareGlobalFont();
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        m_state->CleanupGlobalFont();
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        m_state->PrepareGlobalScreenBuffer();
        m_state->PrepareGlobalInputBuffer();
        m_state->PrepareReadHandle();
        m_state->PrepareCookedReadData();
        m_pHistory = CommandHistory::s_Allocate(L"cmd.exe", (HANDLE)0);
        if (!m_pHistory)
        {
            return false;
        }
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        CommandHistory::s_Free((HANDLE)0);
        m_pHistory = nullptr;
        m_state->CleanupCookedReadData();
        m_state->CleanupReadHandle();
        m_state->CleanupGlobalInputBuffer();
        m_state->CleanupGlobalScreenBuffer();
        return true;
    }

    TEST_METHOD(CanDismiss)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = VK_ESCAPE;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CommandNumberPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        const std::wstring testString = L"hello world";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        const std::wstring resultString(buffer, buffer + testString.size());
        VERIFY_ARE_EQUAL(testString, resultString);
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, testString.size() * sizeof(wchar_t));

        // popup has been dismissed
        VERIFY_IS_FALSE(CommandLine::Instance().HasPopup());
    }

    TEST_METHOD(CanDismissAllPopups)
    {
        Log::Comment(L"that that all popups are dismissed when CommandNumberPopup is dismissed");
        // CommanNumberPopup is the only popup that can act as a 2nd popup. make sure that it dismisses all
        // popups when exiting
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = VK_ESCAPE;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // add popups to CommandLine
        auto& commandLine = CommandLine::Instance();
        commandLine._popups.emplace_front(std::make_unique<CommandListPopup>(gci.GetActiveOutputBuffer(), *m_pHistory));
        commandLine._popups.emplace_front(std::make_unique<CommandNumberPopup>(gci.GetActiveOutputBuffer()));
        auto& numberPopup = *commandLine._popups.front();
        numberPopup.SetUserInputFunction(fn);

        VERIFY_ARE_EQUAL(commandLine._popups.size(), 2u);

        // prepare cookedReadData
        const std::wstring testString = L"hello world";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(numberPopup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        VERIFY_IS_FALSE(commandLine.HasPopup());
    }

    TEST_METHOD(EmptyInputCountsAsOldestHistory)
    {
        Log::Comment(L"hitting enter with no input should grab the oldest history item");
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = false;
            wch = UNICODE_CARRIAGERETURN;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CommandNumberPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should contain the least recent history item

        const std::wstring_view expected = m_pHistory->GetLastCommand();
        const std::wstring resultString(buffer, buffer + expected.size());
        VERIFY_ARE_EQUAL(expected, resultString);
    }

    TEST_METHOD(CanSelectHistoryItem)
    {
        PopupTestHelper::InitHistory(*m_pHistory);
        for (unsigned int historyIndex = 0; historyIndex < m_pHistory->GetNumberOfCommands(); ++historyIndex)
        {
            Popup::UserInputFunction fn = [historyIndex](COOKED_READ_DATA& /*cookedReadData*/,
                                                         bool& popupKey,
                                                         DWORD& modifiers,
                                                         wchar_t& wch) {
                static bool needReturn = false;
                popupKey = false;
                modifiers = 0;
                if (!needReturn)
                {
                    const auto str = std::to_string(historyIndex);
                    wch = str.at(0);
                    needReturn = true;
                }
                else
                {
                    wch = UNICODE_CARRIAGERETURN;
                    needReturn = false;
                }
                return STATUS_SUCCESS;
            };

            auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            // prepare popup
            CommandNumberPopup popup{ gci.GetActiveOutputBuffer() };
            popup.SetUserInputFunction(fn);

            // prepare cookedReadData
            wchar_t buffer[BUFFER_SIZE];
            std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
            auto& cookedReadData = gci.CookedReadData();
            PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
            cookedReadData._commandHistory = m_pHistory;

            VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

            // the buffer should contain the correct nth history item

            const auto expected = m_pHistory->GetNth(gsl::narrow<short>(historyIndex));
            const std::wstring resultString(buffer, buffer + expected.size());
            VERIFY_ARE_EQUAL(expected, resultString);
        }
    }

    TEST_METHOD(LargeNumberGrabsNewestHistoryItem)
    {
        Log::Comment(L"entering a number larger than the number of history items should grab the most recent history item");

        // simulates user pressing 1, 2, 3, 4, 5, enter
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static int num = 1;
            popupKey = false;
            modifiers = 0;
            if (num <= 5)
            {
                const auto str = std::to_string(num);
                wch = str.at(0);
                ++num;
            }
            else
            {
                wch = UNICODE_CARRIAGERETURN;
            }
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CommandNumberPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should contain the most recent history item

        const std::wstring_view expected = m_pHistory->GetLastCommand();
        const std::wstring resultString(buffer, buffer + expected.size());
        VERIFY_ARE_EQUAL(expected, resultString);
    }

    TEST_METHOD(InputIsLimited)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        CommandNumberPopup popup{ gci.GetActiveOutputBuffer() };

        // input can't delete past zero number input
        popup._pop();
        VERIFY_ARE_EQUAL(popup._parse(), 0);

        // input can only be numbers
        VERIFY_THROWS_SPECIFIC(popup._push(L'$'), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });
        VERIFY_THROWS_SPECIFIC(popup._push(L'A'), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_INVALIDARG; });

        // input can't be more than 5 numbers
        popup._push(L'1');
        VERIFY_ARE_EQUAL(popup._parse(), 1);
        popup._push(L'2');
        VERIFY_ARE_EQUAL(popup._parse(), 12);
        popup._push(L'3');
        VERIFY_ARE_EQUAL(popup._parse(), 123);
        popup._push(L'4');
        VERIFY_ARE_EQUAL(popup._parse(), 1234);
        popup._push(L'5');
        VERIFY_ARE_EQUAL(popup._parse(), 12345);
        // this shouldn't affect the parsed number
        popup._push(L'6');
        VERIFY_ARE_EQUAL(popup._parse(), 12345);
        // make sure we can delete input correctly
        popup._pop();
        VERIFY_ARE_EQUAL(popup._parse(), 1234);
    }
};
