// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using namespace WEX::TestExecution;
using namespace WEX::Common;

const DWORD _dwMaxMillisecondsToWaitOnStartup = 120 * 1000;
const DWORD _dwStartupWaitPollingIntervalInMilliseconds = 200;

static PCWSTR pwszConsoleKeyName = L"Console";
static PCWSTR pwszForceV2ValueName = L"ForceV2";

// This class is intended to set up the testing environment against the produced binary
// instead of using the Windows-default copy of console host.

wil::unique_handle hJob;
wil::unique_process_information pi;

static FILE* std_out = nullptr;
static FILE* std_in = nullptr;

// This will automatically try to terminate the job object (and all of the
// binaries under test that are children) whenever this class gets shut down.
// also closes the FILE pointers created by reopening stdin and stdout.
auto OnAppExitKillJob = wil::scope_exit([&] {
    if (std_out != nullptr)
    {
        fclose(std_out);
    }
    if (std_in != nullptr)
    {
        fclose(std_in);
    }
    if (nullptr != hJob.get())
    {
        THROW_LAST_ERROR_IF(!TerminateJobObject(hJob.get(), S_OK));
    }
});

wistd::unique_ptr<CommonV1V2Helper> v2ModeHelper;

BEGIN_MODULE()
    MODULE_PROPERTY(L"WinPerfSource", L"Console")
    MODULE_PROPERTY(L"WinPerf.WPRProfile", L"ConsolePerf.wprp")
    MODULE_PROPERTY(L"WinPerf.WPRProfileId", L"ConsolePerf.Verbose.File")
    MODULE_PROPERTY(L"WinPerf.Regions", L"ConsolePerf.Regions.xml")

    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\sdk\\lib\\minwin\\$arch\\api-ms-win-core-console-l1-2-1.lib")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\sdk\\lib\\minwin\\$arch\\api-ms-win-core-console-l2-2-0.lib")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\sdk\\lib\\minwin\\$arch\\api-ms-win-core-console-l3-2-0.lib")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\mincore\\priv_sdk\\lib\\$arch\\api-ms-win-core-console-ansi-l2-1-0.lib")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\minwin\\priv_sdk\\inc\\conmsgl1.h")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\minwin\\priv_sdk\\inc\\conmsgl2.h")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\minwin\\priv_sdk\\inc\\conmsgl3.h")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\internal\\windows\\inc\\winconp.h")

    // Public
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\external\\sdk\\inc\\wincon.h")
    MODULE_PROPERTY(L"ArtifactUnderTest", L"onecore\\external\\sdk\\inc\\wincontypes.h")

    // Relative to _NTTREE
    MODULE_PROPERTY(L"BinaryUnderTest", L"conhostv1.dll")
    MODULE_PROPERTY(L"BinaryUnderTest", L"conhost.exe")
END_MODULE()

MODULE_SETUP(ModuleSetup)
{
    // The sources files inside windows use a C define to say it's inside windows and we should be
    // testing against the inbox conhost. This is awesome for inbox TAEF RI gate tests so it uses
    // the one generated from the same build.
    bool insideWindows = false;
#ifdef __INSIDE_WINDOWS
    insideWindows = true;
#endif

    bool forceOpenConsole = false;
    RuntimeParameters::TryGetValue(L"ForceOpenConsole", forceOpenConsole);

    if (forceOpenConsole)
    {
        insideWindows = false;
    }

    // Look up a runtime parameter to see if we want to test as v1.
    // This is useful while developing tests to try to see if they run the same on v2 and v1.
    bool testAsV1 = false;
    RuntimeParameters::TryGetValue(L"TestAsV1", testAsV1);

    if (testAsV1)
    {
        v2ModeHelper.reset(new CommonV1V2Helper(CommonV1V2Helper::ForceV2States::V1));
    }
    else
    {
        v2ModeHelper.reset(new CommonV1V2Helper(CommonV1V2Helper::ForceV2States::V2));
    }

    // Retrieve location of directory that the test was deployed to.
    // We're going to look for OpenConsole.exe in the same directory.
    String value;
    VERIFY_SUCCEEDED_RETURN(RuntimeParameters::TryGetValue(L"TestDeploymentDir", value));

    // If inside windows or testing as v1, use the inbox conhost to launch by just specifying the test EXE name.
    // The OS will auto-start the inbox conhost to host this process.
    if (insideWindows || testAsV1)
    {
        value = value.Append(L"Nihilist.exe");
    }
    else
    {
        // If we're outside or testing V2, let's use the open console binary we built.
        value = value.Append(L"OpenConsole.exe Nihilist.exe");
    }

    // Must make mutable string of appropriate length to feed into args.
    size_t const cchNeeded = value.GetLength() + 1;

    // We use regular new (not a smart pointer) and a scope exit delete because CreateProcess needs mutable space
    // and it'd be annoying to const_cast the smart pointer's .get() just for the sake of.
    PWSTR str = new WCHAR[cchNeeded];
    auto cleanStr = wil::scope_exit([&] { if (nullptr != str) { delete[] str; } });

    VERIFY_SUCCEEDED_RETURN(StringCchCopyW(str, cchNeeded, (WCHAR*)value.GetBuffer()));

    // Create a job object to hold the OpenConsole.exe process and the child it creates
    // so we can terminate it easily when we exit.
    hJob.reset(CreateJobObjectW(nullptr, nullptr));
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(nullptr != hJob);

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobLimits = { 0 };
    JobLimits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetInformationJobObject(hJob.get(), JobObjectExtendedLimitInformation, &JobLimits, sizeof(JobLimits)));

    // Setup and call create process.
    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);

    // We start suspended so we can put it in the job before it does anything
    // We say new console so it doesn't run in the same window as our test.
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(CreateProcessW(nullptr,
                                                      str,
                                                      nullptr,
                                                      nullptr,
                                                      FALSE,
                                                      CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
                                                      nullptr,
                                                      nullptr,
                                                      &si,
                                                      pi.addressof()));

    // Put the new OpenConsole process into the job. The default Job system means when OpenConsole
    // calls CreateProcess, its children will automatically join the job.
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(AssignProcessToJobObject(hJob.get(), pi.hProcess));

    // Let the thread run
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(-1 != ResumeThread(pi.hThread));

    // We have to enter a wait loop here to compensate for Code Coverage instrumentation that might be
    // injected into the process. That takes a while.
    DWORD dwTotalWait = 0;

    JOBOBJECT_BASIC_PROCESS_ID_LIST pids;
    pids.NumberOfAssignedProcesses = 2;
    while (dwTotalWait < _dwMaxMillisecondsToWaitOnStartup)
    {
        QueryInformationJobObject(hJob.get(),
                                  JobObjectBasicProcessIdList,
                                  &pids,
                                  sizeof(pids),
                                  nullptr);

        // When there is >1 process in the job, OpenConsole has finally got around to starting cmd.exe.
        // It was held up on instrumentation most likely.
        if (pids.NumberOfAssignedProcesses > 1)
        {
            break;
        }
        else if (pids.NumberOfAssignedProcesses < 1)
        {
            VERIFY_FAIL();
        }

        Sleep(_dwStartupWaitPollingIntervalInMilliseconds);
        dwTotalWait += _dwStartupWaitPollingIntervalInMilliseconds;
    }
    // If it took too long, throw so the test ends here.
    VERIFY_IS_LESS_THAN(dwTotalWait, _dwMaxMillisecondsToWaitOnStartup);

    // Now retrieve the actual list of process IDs in the job.
    DWORD cbRequired = sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + sizeof(ULONG_PTR) * pids.NumberOfAssignedProcesses;
    PJOBOBJECT_BASIC_PROCESS_ID_LIST pPidList = reinterpret_cast<PJOBOBJECT_BASIC_PROCESS_ID_LIST>(HeapAlloc(GetProcessHeap(),
                                                                                                             0,
                                                                                                             cbRequired));
    VERIFY_IS_NOT_NULL(pPidList);
    auto scopeExit = wil::scope_exit([&]() { HeapFree(GetProcessHeap(), 0, pPidList); });

    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(QueryInformationJobObject(hJob.get(),
                                                                 JobObjectBasicProcessIdList,
                                                                 pPidList,
                                                                 cbRequired,
                                                                 nullptr));

    VERIFY_ARE_EQUAL(pids.NumberOfAssignedProcesses, pPidList->NumberOfProcessIdsInList);

    // Dig through the list to find the one that isn't the OpenConsole window and assume it's CMD.exe
    DWORD dwFindPid = 0;
    for (size_t i = 0; i < pPidList->NumberOfProcessIdsInList; i++)
    {
        ULONG_PTR const pidCandidate = pPidList->ProcessIdList[i];

        if (pidCandidate != pi.dwProcessId && 0 != pidCandidate)
        {
            dwFindPid = static_cast<DWORD>(pidCandidate);
            break;
        }
    }

    // If we launched the binary directly, we have to use the PID that we just launched, not search for the other attached one.
    if (insideWindows || testAsV1)
    {
        dwFindPid = pi.dwProcessId;
    }

    // Verify we found a valid pid.
    VERIFY_ARE_NOT_EQUAL(0u, dwFindPid);

    // Now detach from our current console (if we have one) and instead attach
    // to the one that belongs to the CMD.exe in the new OpenConsole.exe window.
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(FreeConsole());

    // Wait a moment for the driver to be ready after freeing to attach.
    Sleep(1000);

    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(AttachConsole(dwFindPid));

    // Replace CRT handles
    // These need to be reopened as read/write or they can affect some of the tests.
    //
    // std_out and std_in need to be closed when tests are finished, this is handled by the wil::scope_exit at the
    // top of this file.
    errno_t err = 0;
    err = freopen_s(&std_out, "CONOUT$", "w+", stdout);
    VERIFY_ARE_EQUAL(0, err);
    err = freopen_s(&std_in, "CONIN$", "r+", stdin);
    VERIFY_ARE_EQUAL(0, err);

    return true;
}

MODULE_CLEANUP(ModuleCleanup)
{
    v2ModeHelper.reset();
    return true;
}
