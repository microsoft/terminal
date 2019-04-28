// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"
#include "PopupTestHelper.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../CopyToCharPopup.hpp"


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
        CommandHistory::s_ClearHistoryListStorage();
        m_state->CleanupCookedReadData();
        m_state->CleanupReadHandle();
        m_state->CleanupGlobalInputBuffer();
        m_state->CleanupGlobalScreenBuffer();
        return true;
    }

    TEST_METHOD(CanDismiss)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](CookedRead& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch)
        {
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
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, testString);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._pCommandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        VERIFY_ARE_EQUAL(testString, cookedReadData._prompt);

        // popup has been dismissed
        VERIFY_IS_FALSE(CommandLine::Instance().HasPopup());
    }

    TEST_METHOD(NothingHappensWhenCharNotFound)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](CookedRead& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch)
        {
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
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._pCommandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        VERIFY_IS_TRUE(cookedReadData._prompt.empty());
    }

    TEST_METHOD(CanCopyToEmptyPrompt)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](CookedRead& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch)
        {
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
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._pCommandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        const std::wstring expectedText = L"here i";

        VERIFY_ARE_EQUAL(expectedText, cookedReadData._prompt);
    }

    TEST_METHOD(WontCopyTextBeforeCursor)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](CookedRead& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch)
        {
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
        const std::wstring testString = L"Whose woods these are I think I know.";
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, testString);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._pCommandHistory = m_pHistory;

        const auto beforeText = cookedReadData._prompt;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // nothing should have changed
        VERIFY_ARE_EQUAL(beforeText, cookedReadData._prompt);
    }

    TEST_METHOD(CanMergeLine)
    {
        // function to simulate user pressing escape key
        Popup::UserInputFunction fn = [](CookedRead& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch)
        {
            popupKey = true;
            wch = L' ';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyToCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData with a string longer than the previous history
        const std::wstring testString = L"fear ";
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, testString);
        PopupTestHelper::InitHistory(*m_pHistory);
        cookedReadData._pCommandHistory = m_pHistory;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        const std::wstring expectedText = L"fear is";
        VERIFY_ARE_EQUAL(expectedText, cookedReadData._prompt);
    }

};
