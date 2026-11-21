// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "CommonState.hpp"

#include "globals.h"
#include "srvinit.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include <TerminalThemeHelpers.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;

class InitTests
{
    TEST_CLASS(InitTests);

    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
    static const UINT s_uiOEMJapaneseCP = 932;
    static const UINT s_uiOEMSimplifiedChineseCP = 936;
    static const UINT s_uiOEMKoreanCP = 949;
    static const UINT s_uiOEMTraditionalChineseCP = 950;

    static const LANGID s_langIdJapanese = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    static const LANGID s_langIdSimplifiedChinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    static const LANGID s_langIdKorean = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);
    static const LANGID s_langIdTraditionalChinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
    static const LANGID s_langIdEnglish = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    // This test exists to ensure the continued behavior of the code in the Windows loader.
    // See the LOAD BEARING CODE comment inside GetConsoleLangId or the investigation results in MSFT: 9808579 for more detail.
    TEST_METHOD(TestGetConsoleLangId)
    {
        using Microsoft::Console::Interactivity::ServiceLocator;
        BEGIN_TEST_METHOD_PROPERTIES()
            // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
            // The interesting ones for us are:
            // Japanese Shift JIS = 932
            // Chinese Simplified GB2312 = 936
            // Korean Unified Hangul = 949
            // Chinese Traditional Big5 = 950
            TEST_METHOD_PROPERTY(L"Data:uiStartupCP", L"{437, 850, 932, 936, 949, 950}")
            TEST_METHOD_PROPERTY(L"Data:uiOutputCP", L"{437, 850, 932, 936, 949, 950}")
        END_TEST_METHOD_PROPERTIES()

        // if ServiceLocator::LocateGlobals().uiWindowsCP = a CJK one
        // we should get SUCCESS and a matching result to our input
        // for any other ServiceLocator::LocateGlobals().uiWindowsCP we should get STATUS_NOT_SUPPORTED and do nothing with the langid.

        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiStartupCP", ServiceLocator::LocateGlobals().uiWindowsCP));

        UINT outputCP;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiOutputCP", outputCP));

        LANGID langId = 0;
        const auto status = GetConsoleLangId(outputCP, &langId);

        if (s_uiOEMJapaneseCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMSimplifiedChineseCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMKoreanCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMTraditionalChineseCP == ServiceLocator::LocateGlobals().uiWindowsCP)
        {
            VERIFY_ARE_EQUAL(STATUS_SUCCESS, status);

            LANGID langIdExpected;
            switch (outputCP)
            {
            case s_uiOEMJapaneseCP:
                langIdExpected = s_langIdJapanese;
                break;
            case s_uiOEMSimplifiedChineseCP:
                langIdExpected = s_langIdSimplifiedChinese;
                break;
            case s_uiOEMKoreanCP:
                langIdExpected = s_langIdKorean;
                break;
            case s_uiOEMTraditionalChineseCP:
                langIdExpected = s_langIdTraditionalChinese;
                break;
            default:
                langIdExpected = s_langIdEnglish;
                break;
            }

            VERIFY_ARE_EQUAL(langIdExpected, langId);
        }
        else
        {
            VERIFY_ARE_EQUAL(STATUS_NOT_SUPPORTED, status);
        }
    }
};

class StandaloneConsolePolicyTests
{
    BEGIN_TEST_CLASS(StandaloneConsolePolicyTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Class")
    END_TEST_CLASS()

    struct PolicyRecorder
    {
        std::vector<DWORD> processIds;
        std::vector<DWORD> boostedProcessIds;
        size_t calls = 0;
        size_t boostCalls = 0;
        HRESULT result = S_OK;
        HRESULT boostResult = S_OK;
        HRESULT boostClearResult = S_OK;
        bool nullProcesses = false;
        bool nullBoostProcesses = false;

        static HRESULT Associate(HWND window, DWORD count, PHANDLE processes)
        {
            auto& recorder = *reinterpret_cast<PolicyRecorder*>(window);
            ++recorder.calls;
            recorder.nullProcesses = processes == nullptr;
            recorder.processIds.clear();
            for (const auto process : std::span{ processes, count })
            {
                recorder.processIds.push_back(GetProcessId(process));
            }
            return recorder.result;
        }

        static HRESULT Boost(HWND window, DWORD count, PHANDLE processes)
        {
            auto& recorder = *reinterpret_cast<PolicyRecorder*>(window);
            ++recorder.boostCalls;
            recorder.nullBoostProcesses = processes == nullptr;
            recorder.boostedProcessIds.clear();
            for (const auto process : std::span{ processes, count })
            {
                recorder.boostedProcessIds.push_back(GetProcessId(process));
            }
            return count ? recorder.boostResult : recorder.boostClearResult;
        }
    };

    TEST_METHOD(TracksExistingNewAndDisconnectedClientsWithoutFocus)
    {
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        const auto originalFlags = console.Flags;
        const auto restoreFlags = wil::scope_exit([&] { console.Flags = originalFlags; });
        WI_ClearFlag(console.Flags, CONSOLE_HAS_FOCUS);

        PolicyRecorder recorder;
        ConsoleProcessList processes;
        const auto cleanup = wil::scope_exit([&] {
            while (const auto process = processes.GetOldestProcess())
            {
                processes.FreeProcessData(process);
            }
        });
        ConsoleProcessHandle* first = nullptr;
        VERIFY_SUCCEEDED(processes.AllocProcessData(GetCurrentProcessId(), GetCurrentThreadId(), 0, &first));
        VERIFY_ARE_EQUAL(size_t{ 0 }, recorder.calls);
        VERIFY_ARE_EQUAL(size_t{ 0 }, recorder.boostCalls);
        VERIFY_SUCCEEDED(processes.ConfigureWindowProcessPolicies(
            reinterpret_cast<HWND>(&recorder), PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.processIds.size());
        VERIFY_ARE_EQUAL(GetCurrentProcessId(), recorder.processIds[0]);
        VERIFY_IS_TRUE(recorder.processIds == recorder.boostedProcessIds);

        wil::unique_process_information child;
        const auto terminate = wil::scope_exit([&] {
            if (child.hProcess && WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT)
            {
                LOG_IF_WIN32_BOOL_FALSE(TerminateProcess(child.hProcess, ERROR_CANCELLED));
                LOG_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0);
            }
        });
        std::wstring command = L"cmd.exe /c exit";
        STARTUPINFOW startup{ sizeof(STARTUPINFOW) };
        VERIFY_IS_TRUE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                                     CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr,
                                     &startup, child.addressof()));
        ConsoleProcessHandle* second = nullptr;
        VERIFY_SUCCEEDED(processes.AllocProcessData(child.dwProcessId, child.dwThreadId, 0, &second));
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.calls);
        processes.ModifyConsoleProcessFocus(false);
        VERIFY_ARE_EQUAL(size_t{ 2 }, recorder.boostCalls);
        VERIFY_ARE_EQUAL(size_t{ 2 }, recorder.processIds.size());
        VERIFY_IS_TRUE(std::ranges::find(recorder.processIds, GetCurrentProcessId()) != recorder.processIds.end());
        VERIFY_IS_TRUE(std::ranges::find(recorder.processIds, child.dwProcessId) != recorder.processIds.end());
        VERIFY_IS_TRUE(recorder.processIds == recorder.boostedProcessIds);

        processes.FreeProcessData(first);
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.processIds.size());
        VERIFY_ARE_EQUAL(child.dwProcessId, recorder.processIds[0]);
        VERIFY_IS_TRUE(recorder.processIds == recorder.boostedProcessIds);
        processes.FreeProcessData(second);
        VERIFY_ARE_EQUAL(size_t{ 0 }, recorder.processIds.size());
        VERIFY_IS_TRUE(recorder.nullProcesses);
        VERIFY_IS_TRUE(recorder.boostedProcessIds.empty());
        VERIFY_IS_TRUE(recorder.nullBoostProcesses);
    }

    TEST_METHOD(ClientPublicationWaitsForConsoleRegistration)
    {
        using namespace Microsoft::Console;
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        wil::unique_handle section{ CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                                       sizeof(ForegroundBoostSharedState), nullptr) };
        VERIFY_IS_NOT_NULL(section.get());
        wil::unique_mapview_ptr<ForegroundBoostSharedState> state{ static_cast<ForegroundBoostSharedState*>(
            MapViewOfFile(section.get(), FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(ForegroundBoostSharedState))) };
        VERIFY_IS_NOT_NULL(state);
        new (state.get()) ForegroundBoostSharedState{};
        wil::unique_event changed;
        changed.create(wil::EventOptions::None);
        wil::unique_handle publisherEvent;
        VERIFY_IS_TRUE(DuplicateHandle(GetCurrentProcess(), changed.get(), GetCurrentProcess(),
                                       publisherEvent.addressof(), EVENT_MODIFY_STATE, FALSE, 0));
        ConsoleProcessList processes;
        ConsoleProcessHandle* client = nullptr;
        const auto cleanup = wil::scope_exit([&] {
            if (client)
            {
                processes.FreeProcessData(client);
            }
        });
        processes.ConfigureForegroundBoostChannel(std::move(section), std::move(publisherEvent));
        VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(changed.get(), 0));
        VERIFY_SUCCEEDED(processes.AllocProcessData(GetCurrentProcessId(), GetCurrentThreadId(), 0, &client));
        VERIFY_ARE_EQUAL(DWORD{ WAIT_TIMEOUT }, WaitForSingleObject(changed.get(), 0));
        ForegroundBoostSnapshot snapshot;
        VERIFY_SUCCEEDED(state->Read(snapshot));
        VERIFY_ARE_EQUAL(LONG{ 0 }, snapshot.count);

        processes.ModifyConsoleProcessFocus(false);
        VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(changed.get(), 0));
        VERIFY_SUCCEEDED(state->Read(snapshot));
        VERIFY_ARE_EQUAL(LONG{ 1 }, snapshot.count);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(), snapshot.clients[0].processId);
        processes.FreeProcessData(client);
        client = nullptr;
        VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(changed.get(), 0));
        VERIFY_SUCCEEDED(state->Read(snapshot));
        VERIFY_ARE_EQUAL(LONG{ 0 }, snapshot.count);
    }

    TEST_METHOD(ReportsUnsupportedAndFailedAssociations)
    {
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        PolicyRecorder recorder;
        ConsoleProcessList processes;
        const auto window = reinterpret_cast<HWND>(&recorder);

        VERIFY_ARE_EQUAL(E_INVALIDARG, processes.ConfigureWindowProcessPolicies(
                                          nullptr, PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_ARE_EQUAL(E_INVALIDARG, processes.ConfigureWindowProcessPolicies(window, nullptr, PolicyRecorder::Boost));
        VERIFY_ARE_EQUAL(E_INVALIDARG, processes.ConfigureWindowProcessPolicies(window, PolicyRecorder::Associate, nullptr));
        VERIFY_ARE_EQUAL(size_t{ 0 }, recorder.calls);
        VERIFY_ARE_EQUAL(size_t{ 0 }, recorder.boostCalls);

        recorder.result = S_FALSE;
        VERIFY_ARE_EQUAL(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED),
                         processes.ConfigureWindowProcessPolicies(window, PolicyRecorder::Associate, PolicyRecorder::Boost));
        recorder.result = E_ACCESSDENIED;
        VERIFY_ARE_EQUAL(E_ACCESSDENIED, processes.ConfigureWindowProcessPolicies(
                                           window, PolicyRecorder::Associate, PolicyRecorder::Boost));
        recorder.result = S_OK;
        VERIFY_SUCCEEDED(processes.ConfigureWindowProcessPolicies(window, PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_ARE_EQUAL(size_t{ 3 }, recorder.calls);
        VERIFY_ARE_EQUAL(size_t{ 3 }, recorder.boostCalls);
        VERIFY_IS_TRUE(recorder.nullProcesses);
    }

    TEST_METHOD(InvalidClientHandleClearsTheAssociation)
    {
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        PolicyRecorder recorder;
        ConsoleProcessList processes;
        const auto cleanup = wil::scope_exit([&] {
            while (const auto process = processes.GetOldestProcess())
            {
                processes.FreeProcessData(process);
            }
        });
        VERIFY_SUCCEEDED(processes.AllocProcessData(0, 0, 0, nullptr));
        VERIFY_ARE_EQUAL(E_HANDLE, processes.ConfigureWindowProcessPolicies(
                                      reinterpret_cast<HWND>(&recorder), PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.calls);
        VERIFY_IS_TRUE(recorder.processIds.empty());
        VERIFY_IS_TRUE(recorder.nullProcesses);
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.boostCalls);
        VERIFY_IS_TRUE(recorder.boostedProcessIds.empty());
        VERIFY_IS_TRUE(recorder.nullBoostProcesses);
    }

    TEST_METHOD(FailedBoostClearsPreviousGroupWithoutDisablingQos)
    {
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        PolicyRecorder recorder;
        ConsoleProcessList processes;
        ConsoleProcessHandle* client = nullptr;
        const auto cleanup = wil::scope_exit([&] {
            if (client)
            {
                processes.FreeProcessData(client);
            }
        });
        VERIFY_SUCCEEDED(processes.AllocProcessData(GetCurrentProcessId(), GetCurrentThreadId(), 0, &client));
        const auto window = reinterpret_cast<HWND>(&recorder);
        VERIFY_SUCCEEDED(processes.ConfigureWindowProcessPolicies(window, PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_IS_TRUE(recorder.processIds == recorder.boostedProcessIds);
        VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.boostedProcessIds.size());

        for (const auto error : { E_ACCESSDENIED, E_INVALIDARG })
        {
            recorder.boostResult = error;
            const auto before = recorder.boostCalls;
            VERIFY_ARE_EQUAL(error, processes.ConfigureWindowProcessPolicies(
                                        window, PolicyRecorder::Associate, PolicyRecorder::Boost));
            VERIFY_ARE_EQUAL(before + 2, recorder.boostCalls);
            VERIFY_IS_TRUE(recorder.boostedProcessIds.empty());
            VERIFY_IS_TRUE(recorder.nullBoostProcesses);
            VERIFY_ARE_EQUAL(size_t{ 1 }, recorder.processIds.size());
            VERIFY_ARE_EQUAL(GetCurrentProcessId(), recorder.processIds[0]);
        }

        recorder.boostResult = S_OK;
        VERIFY_SUCCEEDED(processes.ConfigureWindowProcessPolicies(window, PolicyRecorder::Associate, PolicyRecorder::Boost));
        VERIFY_IS_TRUE(recorder.processIds == recorder.boostedProcessIds);
    }

    TEST_METHOD(NativeWindowPoliciesSucceedWithoutChangingPriorityClass)
    {
        auto& console = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        console.LockConsole();
        const auto unlock = wil::scope_exit([&] { console.UnlockConsole(); });
        const auto window = CreateWindowExW(0, L"STATIC", L"Standalone console policy test", WS_OVERLAPPEDWINDOW,
                                           0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        VERIFY_IS_NOT_NULL(window);
        const auto destroy = wil::scope_exit([&] { DestroyWindow(window); });
        VERIFY_ARE_EQUAL(S_OK, TerminalTrySetWindowAssociatedProcesses(window, 0, nullptr));
        wil::unique_process_information child;
        const auto terminate = wil::scope_exit([&] {
            if (child.hProcess && WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT)
            {
                LOG_IF_WIN32_BOOL_FALSE(TerminateProcess(child.hProcess, ERROR_CANCELLED));
                LOG_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0);
            }
        });
        std::wstring command = L"cmd.exe /c exit";
        STARTUPINFOW startup{ sizeof(STARTUPINFOW) };
        VERIFY_IS_TRUE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                                     CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr,
                                     &startup, child.addressof()));
        Microsoft::Console::Interactivity::ServiceLocator::LocateConsoleControl()->NotifyConsoleApplication(child.dwProcessId);
        ConsoleProcessList processes;
        ConsoleProcessHandle* client = nullptr;
        const auto cleanup = wil::scope_exit([&] {
            if (client)
            {
                processes.FreeProcessData(client);
            }
        });
        const auto priorityClass = GetPriorityClass(child.hProcess);
        VERIFY_ARE_NOT_EQUAL(DWORD{ 0 }, priorityClass);
        VERIFY_SUCCEEDED(processes.AllocProcessData(child.dwProcessId, child.dwThreadId, 0, &client));
        VERIFY_ARE_EQUAL(S_OK, processes.ConfigureWindowProcessPolicies(
                                  window, TerminalTrySetWindowAssociatedProcesses, Microsoft::Console::SetForegroundBoostProcesses));
        VERIFY_ARE_EQUAL(priorityClass, GetPriorityClass(child.hProcess));
        processes.FreeProcessData(client);
        client = nullptr;
        VERIFY_ARE_EQUAL(priorityClass, GetPriorityClass(child.hProcess));
    }
};
