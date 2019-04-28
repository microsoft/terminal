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
        VERIFY_ARE_EQUAL(m_pHistory->GetNumberOfCommands(), 0u);
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

    void VerifyPromptText(CookedRead& cookedReadData, const std::wstring wstr)
    {
        const auto prompt = cookedReadData.Prompt();
        VERIFY_ARE_EQUAL(prompt, wstr);
    }

    void InitCookedReadData(CookedRead& cookedReadData,
                            CommandHistory* pHistory,
                            const std::wstring prompt)
    {
        cookedReadData._pCommandHistory = pHistory;
        cookedReadData._prompt = prompt;
        cookedReadData._promptStartLocation = { 0, 0 };
        cookedReadData._insertionIndex = 0;
    }

    void SetPrompt(CookedRead& cookedReadData, const std::wstring text)
    {
        cookedReadData._prompt = text;
        cookedReadData._insertionIndex = cookedReadData._prompt.size();
    }

    void MoveCursor(CookedRead& cookedReadData, const size_t column)
    {
        cookedReadData._insertionIndex = column;
    }

    TEST_METHOD(CanCycleCommandHistory)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._processHistoryCycling(cookedReadData, CommandHistory::SearchDirection::Next);
        // should not have anything on the prompt
        VERIFY_IS_TRUE(cookedReadData._prompt.empty());

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
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

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
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

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
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

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
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // set current cursor position somewhere in the middle of the prompt
        MoveCursor(cookedReadData, 5);
        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._deletePromptBeforeCursor(cookedReadData);
        //cookedReadData._currentPosition = cursorPos.X;
        VerifyPromptText(cookedReadData, L"word blah");
    }

    TEST_METHOD(CanMoveCursorToEndOfPrompt)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // make sure the cursor is not at the start of the prompt
        VERIFY_ARE_NOT_EQUAL(cookedReadData._insertionIndex, 0u);

        // save current position for later checking
        const auto expectedCursorPos = cookedReadData._insertionIndex;

        MoveCursor(cookedReadData, 0);

        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._moveCursorToEndOfPrompt(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, gsl::narrow<short>(expectedCursorPos));
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, expectedCursorPos);

    }

    TEST_METHOD(CanMoveCursorToStartOfPrompt)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        auto expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // make sure the cursor is not at the start of the prompt
        VERIFY_ARE_NOT_EQUAL(cookedReadData._insertionIndex, 0u);
        VERIFY_IS_FALSE(cookedReadData._prompt.empty());

        auto& commandLine = CommandLine::Instance();
        const COORD cursorPos = commandLine._moveCursorToStartOfPrompt(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, 0);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, 0u);
    }

    TEST_METHOD(CanMoveCursorLeftByWord)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        const std::wstring expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        auto& commandLine = CommandLine::Instance();
        cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().SetPosition({ gsl::narrow<short>(expected.size()), 0 });
        // cursor position at beginning of "blah"
        short expectedPos = 10;
        COORD cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, gsl::narrow<size_t>(expectedPos));

        // move again
        cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().SetPosition({ expectedPos, 0 });
        expectedPos = 5; // before "word"
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, gsl::narrow<size_t>(expectedPos));

        // move again
        cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().SetPosition({ expectedPos, 0 });
        expectedPos = 0; // before "test"
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, gsl::narrow<size_t>(expectedPos));

        // try to move again, nothing should happen
        cookedReadData.ScreenInfo().GetTextBuffer().GetCursor().SetPosition({ expectedPos, 0 });
        cursorPos = commandLine._moveCursorLeftByWord(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, expectedPos);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, gsl::narrow<size_t>(expectedPos));
    }

    TEST_METHOD(CanMoveCursorLeft)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        const std::wstring expected = L"test word blah";
        SetPrompt(cookedReadData, expected);
        VerifyPromptText(cookedReadData, expected);

        // move left from end of prompt text to the beginning of the prompt
        auto& commandLine = CommandLine::Instance();
        for (auto it = expected.crbegin(); it != expected.crend(); ++it)
        {
            const COORD cursorPos = commandLine._moveCursorLeft(cookedReadData);
            VERIFY_ARE_EQUAL(cookedReadData._prompt.at(cookedReadData._insertionIndex), *it);
        }
        // should now be at the start of the prompt
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, 0u);

        // try to move left a final time, nothing should change
        const COORD cursorPos = commandLine._moveCursorLeft(cookedReadData);
        VERIFY_ARE_EQUAL(cursorPos.X, 0);
        VERIFY_ARE_EQUAL(cookedReadData._insertionIndex, 0u);
    }

    /*
      // TODO MSFT:11285829 tcome back and turn these on once the system cursor isn't needed
    TEST_METHOD(CanMoveCursorRightByWord)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

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
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, nullptr, L"");

        auto& commandLine = CommandLine::Instance();
        commandLine._insertCtrlZ(cookedReadData);
        VerifyPromptText(cookedReadData, L"\x1a"); // ctrl-z
    }

    TEST_METHOD(CanDeleteCommandHistory)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 1", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 2", false));
        VERIFY_SUCCEEDED(m_pHistory->Add(L"echo 3", false));

        auto& commandLine = CommandLine::Instance();
        commandLine._deleteCommandHistory(cookedReadData);
        VERIFY_ARE_EQUAL(m_pHistory->GetNumberOfCommands(), 0u);
    }

    TEST_METHOD(CanFillPromptWithPreviousCommandFragment)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

        VERIFY_SUCCEEDED(m_pHistory->Add(L"I'm a little teapot", false));
        SetPrompt(cookedReadData, L"short and stout");

        auto& commandLine = CommandLine::Instance();
        commandLine._fillPromptWithPreviousCommandFragment(cookedReadData);
        VerifyPromptText(cookedReadData, L"short and stoutapot");
    }

    TEST_METHOD(CanCycleMatchingCommandHistory)
    {
        auto& cookedReadData = ServiceLocator::LocateGlobals().getConsoleInformation().CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

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
        auto& consoleInfo = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& screenInfo = consoleInfo.GetActiveOutputBuffer();
        auto& cookedReadData = consoleInfo.CookedReadData();
        InitCookedReadData(cookedReadData, m_pHistory, L"");

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
        for (const auto& wch : text)
        {
            cookedReadData.BufferInput(wch);
        }
        size_t numBytes = 0;
        ULONG ctrlKeyState = 0;
        VERIFY_ARE_EQUAL(cookedReadData.Read(true, numBytes, ctrlKeyState), static_cast<NTSTATUS>(CONSOLE_STATUS_WAIT));

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
