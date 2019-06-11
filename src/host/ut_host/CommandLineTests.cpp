// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "../../interactivity/inc/ServiceLocator.hpp"

#include "../cmdline.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

constexpr size_t PROMPT_SIZE = 512;

class CommandLineTests
{
    std::unique_ptr<CommonState> m_state;
    CommandHistory* m_pHistory;

    TEST_CLASS(CommandLineTests);

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

    void VerifyPromptText(COOKED_READ_DATA& cookedReadData, const std::wstring wstr)
    {
        const auto span = cookedReadData.SpanWholeBuffer();
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, wstr.size() * sizeof(wchar_t));
        for (size_t i = 0; i < wstr.size(); ++i)
        {
            VERIFY_ARE_EQUAL(span.at(i), wstr.at(i));
        }
    }

    void InitCookedReadData(COOKED_READ_DATA& cookedReadData,
                            CommandHistory* pHistory,
                            wchar_t* pBuffer,
                            const size_t cchBuffer)
    {
        cookedReadData._commandHistory = pHistory;
        cookedReadData._userBuffer = pBuffer;
        cookedReadData._userBufferSize = cchBuffer * sizeof(wchar_t);
        cookedReadData._bufferSize = cchBuffer * sizeof(wchar_t);
        cookedReadData._backupLimit = pBuffer;
        cookedReadData._bufPtr = pBuffer;
        cookedReadData._exeName = L"cmd.exe";
        cookedReadData.OriginalCursorPosition() = { 0, 0 };
    }

    void SetPrompt(COOKED_READ_DATA& cookedReadData, const std::wstring text)
    {
        std::copy(text.begin(), text.end(), cookedReadData._backupLimit);
        cookedReadData._bytesRead = text.size() * sizeof(wchar_t);
        cookedReadData._currentPosition = text.size();
        cookedReadData._bufPtr += text.size();
        cookedReadData._visibleCharCount = text.size();
    }

    void MoveCursor(COOKED_READ_DATA& cookedReadData, const size_t column)
    {
        cookedReadData._currentPosition = column;
        cookedReadData._bufPtr = cookedReadData._backupLimit + column;
    }

    TEST_METHOD(CanCycleCommandHistory)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        // should not have anything on the prompt
        VERIFY_ARE_EQUAL(cookedReadData._bytesRead, 0u);

        // go back one history item
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        VerifyPromptText(cookedReadData, L"echo 3");

        // try to go to the next history item, prompt shouldn't change
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        VerifyPromptText(cookedReadData, L"echo 3");

        // go back another
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        VerifyPromptText(cookedReadData, L"echo 2");

        // go forward
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        VerifyPromptText(cookedReadData, L"echo 3");

        // go back two
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        VerifyPromptText(cookedReadData, L"echo 1");

        // make sure we can't go back further
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        VerifyPromptText(cookedReadData, L"echo 1");

        // can still go forward
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        VerifyPromptText(cookedReadData, L"echo 2");
    }

    TEST_METHOD(CanSetPromptToOldestHistory)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._setPromptToOldestCommand(cookedReadData);
        VerifyPromptText(cookedReadData, L"echo 1");

        // change prompt and go back to oldest
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        commandLine._setPromptToOldestCommand(cookedReadData);
        VerifyPromptText(cookedReadData, L"echo 1");
    }

    TEST_METHOD(CanSetPromptToNewestHistory)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._setPromptToNewestCommand(cookedReadData);
        VerifyPromptText(cookedReadData, L"echo 3");

        // change prompt and go back to newest
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Previous);
        commandLine._setPromptToNewestCommand(cookedReadData);
        VerifyPromptText(cookedReadData, L"echo 3");
    }

    TEST_METHOD(CanDeletePromptAfterCursor)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        auto& commandLine = CommandLine::Instance();
        // set current cursor position somewhere in the middle of the prompt
        MoveCursor(cookedReadData, 4);
        commandLine.DeletePromptAfterCursor(cookedReadData);
        VerifyPromptText(cookedReadData, L"test");
    }

    TEST_METHOD(CanDeletePromptBeforeCursor)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // set current cursor position somewhere in the middle of the prompt
        MoveCursor(cookedReadData, 5);
        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._deletePromptBeforeCursor(cookedReadData);
        cookedReadData._currentPosition = cursorPos.X;
        VerifyPromptText(cookedReadData, L"word blah");
    }

    TEST_METHOD(CanMoveCursorToEndOfPrompt)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // make sure the cursor is not at the start of the prompt
        VERIFY_ARE_NOT_EQUAL(cookedReadData._currentPosition, 0u);
        VERIFY_ARE_NOT_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);

        // save current position for later checking
        const auto expectedCursorPos = cookedReadData._currentPosition;
        const auto expectedBufferPos = cookedReadData._bufPtr;

        MoveCursor(cookedReadData, 0);

        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._moveCursorToEndOfPrompt(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, gsl::narrow<short>(expectedCursorPos));
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, expectedCursorPos);
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, expectedBufferPos);
    }

    TEST_METHOD(CanMoveCursorToStartOfPrompt)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // make sure the cursor is not at the start of the prompt
        VERIFY_ARE_NOT_EQUAL(cookedReadData._currentPosition, 0u);
        VERIFY_ARE_NOT_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);

        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._moveCursorToStartOfPrompt(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, 0);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, 0u);
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);
    }

    TEST_METHOD(CanMoveCursorLeftByWord)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        auto& commandLine = CommandLine::Instance();
        // cursor position at beginning of "blah"
        short expectedPos = 10;
        COORD cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, gsl::narrow<size_t>(expectedPos));
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit + expectedPos);

        // move again
        expectedPos = 5; // before "word"
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, gsl::narrow<size_t>(expectedPos));
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit + expectedPos);

        // move again
        expectedPos = 0; // before "test"
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, gsl::narrow<size_t>(expectedPos));
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit + expectedPos);

        // try to move again, nothing should happen
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, gsl::narrow<size_t>(expectedPos));
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit + expectedPos);
    }

    TEST_METHOD(CanMoveCursorLeft)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        const std::wstring expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // move left from end of prompt text to the beginning of the prompt
        auto& commandLine = CommandLine::Instance();
        for (auto it = expected.crbegin(); it != expected.crend(); ++it)
        {
            const COORD cursorPos = commandLine._moveCursorLeft(cookedReadData);
            VERIFY_ARE_EQUAL(*cookedReadData._bufPtr, *it);
        }
        // should now be at the start of the prompt
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, 0u);
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);

        // try to move left a final time, nothing should change
        const COORD cursorPos = commandLine._moveCursorLeft(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, 0);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, 0u);
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, cookedReadData._backupLimit);
    }

    /*
      // TODO MSFT:11285829 tcome back and turn these on once the system cursor isn't needed
    TEST_METHOD(CanMoveCursorRightByWord)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // save current position for later checking
        const auto endCursorPos = cookedReadData._currentPosition;
        const auto endBufferPos = cookedReadData._bufPtr;
        // NOTE: need to initialize the actualy cursor and keep it up to date with the changes here. remove
        once functions are fixed
        // try to move right, nothing should happen
        short expectedPos = gsl::narrow<short>(endCursorPos);
        COORD cursorPos = MoveCursorRightByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._currentPosition, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._bufPtr, endBufferPos);

        // move to beginning of prompt and walk to the right by word
    }

    TEST_METHOD(CanMoveCursorRight)
    {
    }

    TEST_METHOD(CanDeleteFromRightOfCursor)
    {
    }

    */

    TEST_METHOD(CanInsertCtrlZ)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, buffer.get(), PROMPT_SIZE);

        auto& commandLine = CommandLine::Instance();
        commandLine._insertCtrlZ(cookedReadData);
        VerifyPromptText(cookedReadData, L"\x1a"); // ctrl-z
    }

    TEST_METHOD(CanDeleteCommandHistory)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._deleteCommandHistory(cookedReadData);
        VERIFY_ARE_EQUAL(m_pHistory->GetNumberOfCommands(), 0u);
    }

    TEST_METHOD(CanFillPromptWithPreviousCommandFragment)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"I'm a little teapot", false));
        SetPrompt(cookedReadData, L"short and stout");

        auto& commandLine = CommandLine::Instance();
        commandLine._fillPromptWithPreviousCommandFragment(cookedReadData);
        VerifyPromptText(cookedReadData, L"short and stoutapot");
    }

    TEST_METHOD(CanCycleMatchingCommandHistory)
    {
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());

        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        VERIFY_SUCCEEDED(m_pHistory->Add(L"I'm a little teapot", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"short and stout", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"inflammable", false));

        SetPrompt(cookedReadData, L"i");

        auto& commandLine = CommandLine::Instance();
        commandLine._cycleMatchingCommandHistoryToPrompt(cookedReadData);
        VerifyPromptText(cookedReadData, L"inflammable");

        // make sure we skip to the next "i" history item
        commandLine._cycleMatchingCommandHistoryToPrompt(cookedReadData);
        VerifyPromptText(cookedReadData, L"I'm a little teapot");

        // should cycle back to the start of the command history
        commandLine._cycleMatchingCommandHistoryToPrompt(cookedReadData);
        VerifyPromptText(cookedReadData, L"inflammable");
    }

    TEST_METHOD(CmdlineCtrlHomeFullwidthChars)
    {
        Log::Comment(L"Set up buffers, create cooked read data, get screen information.");
        auto buffer = std::make_unique<wchar_t[]>(PROMPT_SIZE);
        VERIFY_IS_NOT_NULL(buffer.get());
        auto& consoleInfo = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = consoleInfo.GetActiveOutputBuffer();
        auto& cookedReadData = consoleInfo.CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, buffer.get(), PROMPT_SIZE);

        Log::Comment(L"Create Japanese text string and calculate the distance we expect the cursor to move.");
        const std::wstring text(L"\x30ab\x30ac\x30ad\x30ae\x30af"); // katakana KA GA KI GI KU
        const auto bufferSize = screenInfo.GetBufferSize();
        const auto cursorBefore = screenInfo.GetTextBuffer().GetCursor().GetPosition();
        auto cursorAfterExpected = cursorBefore;
        for (size_t i = 0; i < text.length() * 2; i++)
        {
            bufferSize.IncrementInBounds(cursorAfterExpected);
        }

        Log::Comment(L"Write the text into the buffer using the cooked read structures as if it came off of someone's input.");
        const auto written = cookedReadData.Write(text);
        VERIFY_ARE_EQUAL(text.length(), written);

        Log::Comment(L"Retrieve the position of the cursor after insertion and check that it moved as far as we expected.");
        const auto cursorAfter = screenInfo.GetTextBuffer().GetCursor().GetPosition();
        VERIFY_ARE_EQUAL(cursorAfterExpected, cursorAfter);

        Log::Comment(L"Walk through the screen buffer data and ensure that the text we wrote filled the cells up as we expected (2 cells per fullwidth char)");
        {
            auto cellIterator = screenInfo.GetCellDataAt(cursorBefore);
            for (size_t i = 0; i < text.length() * 2; i++)
            {
                // Our original string was 5 wide characters which we expected to take 10 cells.
                // Therefore each index of the original string will be used twice ( divide by 2 ).
                const auto expectedTextValue = text.at(i / 2);
                const String expectedText(&expectedTextValue, 1);

                const auto actualTextValue = cellIterator->Chars();
                const String actualText(actualTextValue.data(), gsl::narrow<int>(actualTextValue.size()));

                VERIFY_ARE_EQUAL(expectedText, actualText);
                cellIterator++;
            }
        }

        Log::Comment(L"Now perform the command that is triggered with Ctrl+Home keys normally to erase the entire edit line.");
        auto& commandLine = CommandLine::Instance();
        commandLine._deletePromptBeforeCursor(cookedReadData);

        Log::Comment(L"Check that the entire span of the buffer where we had the fullwidth text is now cleared out and full of blanks (nothing left behind).");
        {
            auto cursorPos = cursorBefore;
            auto cellIterator = screenInfo.GetCellDataAt(cursorPos);

            while (Utils::s_CompareCoords(cursorPos, cursorAfter) < 0)
            {
                const String expectedText(L"\x20"); // unicode space character

                const auto actualTextValue = cellIterator->Chars();
                const String actualText(actualTextValue.data(), gsl::narrow<int>(actualTextValue.size()));

                VERIFY_ARE_EQUAL(expectedText, actualText);
                cellIterator++;

                bufferSize.IncrementInBounds(cursorPos);
            }
        }
    }
};
