// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "search.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using Microsoft::Console::Interactivity::ServiceLocator;

class HistoryTests
{
    TEST_CLASS(HistoryTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.SetNumberOfHistoryBuffers(s_NumberOfBuffers);
        gci.SetHistoryBufferSize(s_BufferSize);
        return true;
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
        return true;
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // Get a fresh storage for each test since it's stored internally as a persistent static for the lifetime of the session.
        CommandHistory::s_ClearHistoryListStorage();
        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        return true;
    }

    TEST_METHOD(AllocateAndFreeOneApp)
    {
        const std::wstring app{ L"testapp1.exe" };
        const HANDLE handle = _MakeHandle(0);

        const auto history = CommandHistory::s_Allocate(app, handle);
        VERIFY_IS_NOT_NULL(history);

        VERIFY_IS_TRUE(WI_IsFlagSet(history->Flags, CommandHistory::CLE_ALLOCATED));
        VERIFY_ARE_EQUAL(1ul, CommandHistory::s_historyLists.size());

        CommandHistory::s_Free(handle);
        // We preserve the app history list for re-use if it reattaches in this session and doesn't age out.
        VERIFY_IS_TRUE(WI_IsFlagClear(history->Flags, CommandHistory::CLE_ALLOCATED), L"Shouldn't actually be gone, just deallocated.");
        VERIFY_ARE_EQUAL(1ul, CommandHistory::s_historyLists.size());
    }

    TEST_METHOD(AllocateTooManyApps)
    {
        VERIFY_IS_LESS_THAN(s_NumberOfBuffers, _manyApps.size(), L"Make sure we declared too many apps for the necessary size.");

        for (size_t i = 0; i < _manyApps.size(); i++)
        {
            CommandHistory::s_Allocate(_manyApps[i], _MakeHandle(i));
        }

        VERIFY_ARE_EQUAL(s_NumberOfBuffers, CommandHistory::s_CountOfHistories(), L"We should have maxed out histories.");

        Log::Comment(L"Since they were all in use, the last app shouldn't have made an entry");
        for (size_t i = 0; i < _manyApps.size() - 1; i++)
        {
            VERIFY_IS_NOT_NULL(CommandHistory::s_FindByExe(_manyApps[i]));
        }

        VERIFY_IS_NULL(CommandHistory::s_FindByExe(_manyApps[4]), L"Verify we can't find the last app.");
    }

    TEST_METHOD(EnsureHistoryRestoredAfterClientLeavesAndRejoins)
    {
        const HANDLE h = _MakeHandle(0);
        Log::Comment(L"Allocate a history and fill it with items.");
        auto history = CommandHistory::s_Allocate(_manyApps[0], h);
        VERIFY_IS_NOT_NULL(history);

        for (size_t i = 0; i < s_BufferSize; i++)
        {
            VERIFY_SUCCEEDED(history->Add(_manyHistoryItems[i], false));
        }

        VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands(), L"Ensure that it is filled.");

        Log::Comment(L"Free it and recreate it with the same name.");
        CommandHistory::s_Free(h);

        // Using a different handle on purpose. Handle shouldn't matter.
        history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(14));
        VERIFY_IS_NOT_NULL(history);

        VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands(), L"Ensure that we still have full commands after freeing and reallocating, same app name, different handle ID");
    }

    TEST_METHOD(TooManyAppsDoesntTakeList)
    {
        Log::Comment(L"Fill up the number of buffers and each history list to the max.");
        for (size_t i = 0; i < s_NumberOfBuffers; i++)
        {
            auto history = CommandHistory::s_Allocate(_manyApps[i], _MakeHandle(i));
            VERIFY_IS_NOT_NULL(history);
            for (size_t j = 0; j < s_BufferSize; j++)
            {
                VERIFY_SUCCEEDED(history->Add(_manyHistoryItems[j], false));
            }
            VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands());
        }
        VERIFY_ARE_EQUAL(s_NumberOfBuffers, CommandHistory::s_historyLists.size());

        Log::Comment(L"Add one more app and it should re-use a buffer but it should be clear.");
        auto history = CommandHistory::s_Allocate(_manyApps[4], _MakeHandle(444));
        VERIFY_IS_NULL(history);
        VERIFY_ARE_EQUAL(s_NumberOfBuffers, CommandHistory::s_historyLists.size());
    }

    TEST_METHOD(AppNamesMatchInsensitive)
    {
        auto history = CommandHistory::s_Allocate(L"testApp", _MakeHandle(777));
        VERIFY_IS_NOT_NULL(history);
        VERIFY_IS_TRUE(history->IsAppNameMatch(L"TEsTaPP"));
    }

    TEST_METHOD(ReallocUp)
    {
        Log::Comment(L"Allocate and fill with too many items.");
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);
        for (size_t j = 0; j < _manyHistoryItems.size(); j++)
        {
            VERIFY_SUCCEEDED(history->Add(_manyHistoryItems[j], false));
        }
        VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands());

        Log::Comment(L"Retrieve items/order.");
        std::vector<std::wstring> commandsStored;
        for (SHORT i = 0; i < (SHORT)history->GetNumberOfCommands(); i++)
        {
            commandsStored.emplace_back(history->GetNth(i));
        }

        Log::Comment(L"Reallocate larger and ensure items and order are preserved.");
        history->Realloc(_manyHistoryItems.size());
        VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands());
        for (SHORT i = 0; i < (SHORT)commandsStored.size(); i++)
        {
            VERIFY_ARE_EQUAL(String(commandsStored[i].data()), String(history->GetNth(i).data()));
        }

        Log::Comment(L"Fill up the larger buffer and ensure they fit this time.");
        for (size_t j = 0; j < _manyHistoryItems.size(); j++)
        {
            VERIFY_SUCCEEDED(history->Add(_manyHistoryItems[j], false));
        }
        VERIFY_ARE_EQUAL(_manyHistoryItems.size(), history->GetNumberOfCommands());
    }

    TEST_METHOD(ReallocDown)
    {
        Log::Comment(L"Allocate and fill with just enough items.");
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);
        for (size_t j = 0; j < s_BufferSize; j++)
        {
            VERIFY_SUCCEEDED(history->Add(_manyHistoryItems[j], false));
        }
        VERIFY_ARE_EQUAL(s_BufferSize, history->GetNumberOfCommands());

        Log::Comment(L"Retrieve items/order.");
        std::vector<std::wstring> commandsStored;
        for (SHORT i = 0; i < (SHORT)history->GetNumberOfCommands(); i++)
        {
            commandsStored.emplace_back(history->GetNth(i));
        }

        Log::Comment(L"Reallocate smaller and ensure items and order are preserved. Items at end of list should be trimmed.");
        history->Realloc(5);
        for (SHORT i = 0; i < 5; i++)
        {
            VERIFY_ARE_EQUAL(String(commandsStored[i].data()), String(history->GetNth(i).data()));
        }
    }

    TEST_METHOD(AddSequentialDuplicates)
    {
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);

        // The same command twice is always suppressed.
        VERIFY_SUCCEEDED(history->Add(L"dir", false));
        VERIFY_SUCCEEDED(history->Add(L"dir", false));

        VERIFY_ARE_EQUAL(1ul, history->GetNumberOfCommands());
    }

    TEST_METHOD(AddSequentialNoDuplicates)
    {
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);

        // The same command twice is always suppressed.
        VERIFY_SUCCEEDED(history->Add(L"dir", true));
        VERIFY_SUCCEEDED(history->Add(L"dir", true));

        VERIFY_ARE_EQUAL(1ul, history->GetNumberOfCommands());
    }

    TEST_METHOD(AddNonsequentialDuplicates)
    {
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);

        // Duplicates not suppressed here. Dir (3rd line) will not replace/merge with 1st line.
        VERIFY_SUCCEEDED(history->Add(L"dir", false));
        VERIFY_SUCCEEDED(history->Add(L"cd", false));
        VERIFY_SUCCEEDED(history->Add(L"dir", false));

        VERIFY_ARE_EQUAL(3ul, history->GetNumberOfCommands());
    }

    TEST_METHOD(AddNonsequentialNoDuplicates)
    {
        auto history = CommandHistory::s_Allocate(_manyApps[0], _MakeHandle(0));
        VERIFY_IS_NOT_NULL(history);

        // Duplicates suppressed here. Dir (3rd line) will replace/merge with 1st line.
        VERIFY_SUCCEEDED(history->Add(L"dir", true));
        VERIFY_SUCCEEDED(history->Add(L"cd", false));
        VERIFY_SUCCEEDED(history->Add(L"dir", true));

        VERIFY_ARE_EQUAL(2ul, history->GetNumberOfCommands());
    }

private:
    const std::array<std::wstring, 5> _manyApps = {
        L"foo.exe",
        L"bar.exe",
        L"baz.exe",
        L"apple.exe",
        L"banana.exe"
    };

    const std::array<std::wstring, 12> _manyHistoryItems = {
        L"dir",
        L"dir /w",
        L"dir /p /w",
        L"telnet 127.0.0.1",
        L"ipconfig",
        L"ipconfig /all",
        L"net",
        L"ping 127.0.0.1",
        L"cd ..",
        L"bcz",
        L"notepad sources",
        L"git push"
    };

    static constexpr UINT s_NumberOfBuffers = 4;
    static constexpr UINT s_BufferSize = 10;

    HANDLE _MakeHandle(size_t index)
    {
        return reinterpret_cast<HANDLE>((index + 1) * 4);
    }
};
