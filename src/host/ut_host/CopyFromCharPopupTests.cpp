// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"
#include "PopupTestHelper.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../CopyFromCharPopup.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

static constexpr size_t BUFFER_SIZE = 256;

class CopyFromCharPopupTests
{
    TEST_CLASS(CopyFromCharPopupTests);

    std::unique_ptr<CommonState> m_state;

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
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
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
        CopyFromCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        std::wstring testString = L"hello world";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // the buffer should not be changed
        std::wstring resultString(buffer, buffer + testString.size());
        VERIFY_ARE_EQUAL(testString, resultString);
        VERIFY_ARE_EQUAL(cookedReadData.BytesRead(), testString.size() * sizeof(wchar_t));

        // popup has been dismissed
        VERIFY_IS_FALSE(CommandLine::Instance().HasPopup());
    }

    TEST_METHOD(DeleteAllWhenCharNotFound)
    {
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = false;
            wch = L'x';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyFromCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        std::wstring testString = L"hello world";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        // move cursor to beginning of prompt text
        cookedReadData.InsertionPoint() = 0;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        // all text to the right of the cursor should be gone
        VERIFY_ARE_EQUAL(cookedReadData.BytesRead(), 0u);
    }

    TEST_METHOD(CanDeletePartialLine)
    {
        Popup::UserInputFunction fn = [](COOKED_READ_DATA& /*cookedReadData*/, bool& popupKey, DWORD& modifiers, wchar_t& wch) {
            popupKey = false;
            wch = L'f';
            modifiers = 0;
            return STATUS_SUCCESS;
        };

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // prepare popup
        CopyFromCharPopup popup{ gci.GetActiveOutputBuffer() };
        popup.SetUserInputFunction(fn);

        // prepare cookedReadData
        std::wstring testString = L"By the rude bridge that arched the flood";
        wchar_t buffer[BUFFER_SIZE];
        std::fill(std::begin(buffer), std::end(buffer), UNICODE_SPACE);
        std::copy(testString.begin(), testString.end(), std::begin(buffer));
        auto& cookedReadData = gci.CookedReadData();
        PopupTestHelper::InitReadData(cookedReadData, buffer, ARRAYSIZE(buffer), testString.size());
        // move cursor to index 12
        const size_t index = 12;
        cookedReadData.SetBufferCurrentPtr(buffer + index);
        cookedReadData.InsertionPoint() = index;

        VERIFY_ARE_EQUAL(popup.Process(cookedReadData), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT_NO_BLOCK));

        std::wstring expectedText = L"By the rude flood";
        VERIFY_ARE_EQUAL(cookedReadData.BytesRead(), expectedText.size() * sizeof(wchar_t));
        std::wstring resultText(buffer, buffer + expectedText.size());
        VERIFY_ARE_EQUAL(resultText, expectedText);
    }
};
