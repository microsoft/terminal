// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"
#include "PopupTestHelper.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../CopyToCharPopup.hpp"

using Microsoft::Console::Interactivity::ServiceLocator;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

static constexpr size_t BUFFER_SIZE = 256;

class CopyToCharPopupTests
{
    TEST_CLASS(CopyToCharPopupTests);

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
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
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

    TEST_METHOD(NothingHappensWhenCharNotFound)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = L'x';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0u);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, 0u);
    }

    TEST_METHOD(CanCopyToEmptyPrompt)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = L's';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), 0u);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        const std::wstring expectedText = L"here i";

        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit + expectedText.size());
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, expectedText.size() * sizeof(wchar_t));

        // make sure that the text matches
        const std::wstring resultText(buffer, buffer + expectedText.size());
        VERIFY_ARE_EQUAL(resultText, expectedText);
        // make sure that more wasn't copied
        VERIFY_ARE_EQUAL(buffer[expectedText.size()], UNICODE_SPACE);
    }

    TEST_METHOD(WontCopyTextBeforeCursor)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = L's';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData with a string longer than the previous history
        const std::wstring testString = L"Whose woods there are I think I know.";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        const wchar_t* const expectedBufPtr = cookedReadData._bufPtr;
        const size_t expectedBytesRead = cookedReadData._bytesRead;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // nothing should have changed
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, expectedBufPtr);
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, expectedBytesRead);
        const std::wstring resultText(buffer, buffer + testString.size());
        VERIFY_ARE_EQUAL(resultText, testString);
        // make sure that more wasn't copied
        VERIFY_ARE_EQUAL(buffer[testString.size()], UNICODE_SPACE);
    }

    TEST_METHOD(CanMergeLine)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = true;
            wch = L's';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData with a string longer than the previous history
        const std::wstring testString = L"fear ";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._commandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        const std::wstring expectedText = L"fear is";
        const std::wstring resultText(buffer, buffer + testString.size());
        VERIFY_ARE_EQUAL(resultText, testString);
        // make sure that more wasn't copied
        VERIFY_ARE_EQUAL(buffer[expectedText.size()], UNICODE_SPACE);
    }
};
