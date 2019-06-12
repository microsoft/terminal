// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../CommandListPopup.hpp"
#include "PopupTestHelper.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;
static constexpr size_t BUFFER_SIZE = 256;
static constexpr UINT s_NumberOfHistoryBuffers = 4;
static constexpr UINT s_HistoryBufferSize = 50;

class CommandListPopupTests
{
    TEST_CLASS(CommandListPopupTests);

    std::unique_ptr<CommonState> m_state;
    CommandHistory* m_pHistory;

    TEST_CLASS_SETUP(ClassSetup)
    {
        m_state = std::make_unique<CommonState>();
        m_state->PrepareGlobalFont();

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetNumberOfHistoryBuffers(s_NumberOfHistoryBuffers);
        gci.SetHistoryBufferSize(s_HistoryBufferSize);
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
        // resize command history storage to 50 items so that we don't cycle on accident
        // when PopupTestHelper::InitLongHistory() is called.
        CommandHistory::s_ResizeAll(50);
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

    void InitReadData(COOKED_READ_DATA& cookedReadData,
                      wchar_t* const pBuffer,
                      const size_t cchBuffer,
                      const size_t cursorPosition)
    {
        cookedReadData._bufferSize = cchBuffer * sizeof(wchar_t);
        cookedReadData._bufPtr = pBuffer + cursorPosition;
        cookedReadData._backupLimit = pBuffer;
        cookedReadData.OriginalCursorPosition() = { 0, 0 };
        cookedReadData._bytesRead = cursorPosition * sizeof(wchar_t);
        cookedReadData._currentPosition = cursorPosition;
        cookedReadData.VisibleCharCount() = cursorPosition;
    }

    TEST_METHOD(CanDismiss)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            modifiers = 0;
            wch = VK_ESCAPE;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        const std::wstring testString = L"hello world";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        const std::wstring resultString(buffer, buffer + testString.size());
        VERIFY_ARE_EQUAL(testString, resultString);
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, testString.size() * sizeof(wchar_t));

        // popup has been dismissed
        VERIFY_IS_FALSE(CommandLine::Instance().HasPopup());
    }

    TEST_METHOD(UpMovesSelection)
    {
        // function to simulate user pressing up arrow
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_UP;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        const short commandNumberBefore = popup._currentCommand;
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved up one line
        VERIFY_ARE_EQUAL(commandNumberBefore - 1, popup._currentCommand);
    }

    TEST_METHOD(DownMovesSelection)
    {
        // function to simulate user pressing down arrow
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_DOWN;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);
        // set the current command selection to the top of the list
        popup._currentCommand = 0;

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        const short commandNumberBefore = popup._currentCommand;
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved down one line
        VERIFY_ARE_EQUAL(commandNumberBefore + 1, popup._currentCommand);
    }

    TEST_METHOD(EndMovesSelectionToEnd)
    {
        // function to simulate user pressing end key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_END;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);
        // set the current command selection to the top of the list
        popup._currentCommand = 0;

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved to the bottom line
        VERIFY_ARE_EQUAL(m_pHistory->GetNumberOfCommands() - 1, gsl::narrow<size_t>(popup._currentCommand));
    }

    TEST_METHOD(HomeMovesSelectionToStart)
    {
        // function to simulate user pressing home key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_HOME;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved to the bottom line
        VERIFY_ARE_EQUAL(0, popup._currentCommand);
    }

    TEST_METHOD(PageUpMovesSelection)
    {
        // function to simulate user pressing page up key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_PRIOR;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitLongHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved up a page
        VERIFY_ARE_EQUAL(static_cast<short>(m_pHistory->GetNumberOfCommands()) - popup.Height() - 1, popup._currentCommand);
    }

    TEST_METHOD(PageDownMovesSelection)
    {
        // function to simulate user pressing page down key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_NEXT;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitLongHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);
        // set the current command selection to the top of the list
        popup._currentCommand = 0;

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // selection should have moved up a page
        VERIFY_ARE_EQUAL(popup.Height(), popup._currentCommand);
    }

    TEST_METHOD(SideArrowsFillsPrompt)
    {
        // function to simulate user pressing right arrow key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            wch = VK_RIGHT;
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);
        // set the current command selection to the top of the list
        popup._currentCommand = 0;

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        // prompt should have history item in prompt
        const std::wstring_view historyItem = m_pHistory->GetLastCommand();
        const std::wstring_view resultText{ buffer, historyItem.size() };
        VERIFY_ARE_EQUAL(historyItem, resultText);
    }

    TEST_METHOD(CanLaunchCommandNumberPopup)
    {
        // function to simulate user pressing F9
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            wch = VK_F9;
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        auto& commandLine = CommandLine::Instance();
        VERIFY_IS_FALSE(commandLine.HasPopup());
        // should spawn a CommandNumberPopup
        auto scopeExit = wil::scope_exit([&]() { commandLine.EndAllPopups(); });
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT));
        VERIFY_IS_TRUE(commandLine.HasPopup());
    }

    TEST_METHOD(CanDeleteFromCommandHistory)
    {
        // function to simulate user pressing the delete key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_DELETE;
                firstTime = false;
            }
            else
            {
                wch = VK_ESCAPE;
            }
            popupKey = true;
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        const size_t startHistorySize = m_pHistory->GetNumberOfCommands();
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        VERIFY_ARE_EQUAL(m_pHistory->GetNumberOfCommands(), startHistorySize - 1);
    }

    TEST_METHOD(CanReorderHistoryUp)
    {
        // function to simulate user pressing shift + up arrow
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static bool firstTime = true;
            if (firstTime)
            {
                wch = VK_UP;
                firstTime = false;
                modifiers = SHIFT_PRESSED;
            }
            else
            {
                wch = VK_ESCAPE;
                modifiers = 0;
            }
            popupKey = true;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(m_pHistory->GetLastCommand(), L"here is my spout");
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        VERIFY_ARE_EQUAL(m_pHistory->GetLastCommand(), L"here is my handle");
        VERIFY_ARE_EQUAL(m_pHistory->GetNth(2), L"here is my spout");
    }

    TEST_METHOD(CanReorderHistoryDown)
    {
        // function to simulate user pressing the up arrow, then shift + down arrow, then escape
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            static unsigned int count = 0;
            if (count == 0)
            {
                wch = VK_UP;
                modifiers = 0;
            }
            else if (count == 1)
            {
                wch = VK_DOWN;
                modifiers = SHIFT_PRESSED;
            }
            else
            {
                wch = VK_ESCAPE;
                modifiers = 0;
            }
            popupKey = true;
            ++count;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        PopupTestHelper::InitHistory(*m_pHistory);
        CommandListPopup popup{ gci.GetActiveOutputBuffer(), *m_pHistory };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(m_pHistory->GetLastCommand(), L"here is my spout");
        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));
        VERIFY_ARE_EQUAL(m_pHistory->GetLastCommand(), L"here is my handle");
        VERIFY_ARE_EQUAL(m_pHistory->GetNth(2), L"here is my spout");
    }
};
