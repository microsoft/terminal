// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using WEX::Logging::Log;
using namespace WEX::Common;

// This class is intended to provide a canary (simple launch test)
// to ensure that activation of the console still works.
class CanaryTests
{
    BEGIN_TEST_CLASS(CanaryTests)
    END_TEST_CLASS()

    TEST_METHOD(LaunchV1Console);
};

static PCWSTR pwszCmdPath = L"%WINDIR%\\system32\\cmd.exe";
static PCSTR pszCmdGreeting = "Microsoft Windows [Version";

static PCWSTR pwszConhostV1Path = L"%WINDIR%\\system32\\conhostv1.dll";

void CanaryTests::LaunchV1Console()
{
    // First ensure that this system has the v1 console to test.
    std::wstring ConhostV1Path = wil::ExpandEnvironmentStringsW<std::wstring>(pwszConhostV1Path);

    if (!CheckIfFileExists(ConhostV1Path.c_str()))
    {
        Log::Comment(L"This system does not have the legacy conhostv1.dll module. Skipping test.");
        Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    // This will set the console to v1 mode, backing up the current state and restoring when it goes out of scope.
    CommonV1V2Helper SetV1ConsoleHelper(CommonV1V2Helper::ForceV2States::V1);

    // Attempt to launch CMD.exe in a new window
    // Expand any environment variables present in the command line string.
    std::wstring CmdLine = wil::ExpandEnvironmentStringsW<std::wstring>(pwszCmdPath);

    // Create output handle for redirection. We'll read from it to make sure CMD started correctly.
    // We'll let it have a default input handle to make sure it binds to the new console host window that will be created.
    wil::unique_handle OutPipeRead;
    wil::unique_handle OutPipeWrite;
    SECURITY_ATTRIBUTES InheritableSecurity;
    InheritableSecurity.nLength = sizeof(InheritableSecurity);
    InheritableSecurity.lpSecurityDescriptor = nullptr;
    InheritableSecurity.bInheritHandle = TRUE;
    VERIFY_WIN32_BOOL_SUCCEEDED(CreatePipe(&OutPipeRead, &OutPipeWrite, &InheritableSecurity, 0));

    // Create Job object to ensure child will be killed when test ends.
    wil::unique_handle CanaryJob(CreateJobObjectW(nullptr, nullptr));

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobLimits = { 0 };
    JobLimits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetInformationJobObject(CanaryJob.get(), JobObjectExtendedLimitInformation, &JobLimits, sizeof(JobLimits)));

    // Call create process
    STARTUPINFOEX StartupInformation = { 0 };
    StartupInformation.StartupInfo.cb = sizeof(STARTUPINFOEX);
    StartupInformation.StartupInfo.hStdOutput = OutPipeWrite.get();
    StartupInformation.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    wil::unique_process_information ProcessInformation;
    VERIFY_WIN32_BOOL_SUCCEEDED(CreateProcessW(NULL,
                                               CmdLine.data(),
                                               NULL,
                                               NULL,
                                               TRUE,
                                               CREATE_NEW_CONSOLE,
                                               NULL,
                                               NULL,
                                               &StartupInformation.StartupInfo,
                                               ProcessInformation.addressof()));

    // Attach process to job so it dies when we exit this test scope and the handle is released.
    VERIFY_WIN32_BOOL_SUCCEEDED(AssignProcessToJobObject(CanaryJob.get(), ProcessInformation.hProcess));

    // Release our ownership of the Write side of the Out Pipe now that it has been transferred to the child process.
    OutPipeWrite.reset();

    // Wait a second for work to happen.
    Sleep(1000);

    // The process should still be running and active.
    DWORD dwExitCode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode));

    VERIFY_ARE_EQUAL(STILL_ACTIVE, dwExitCode);

    // Read out our redirected output to see that CMD's startup greeting has been printed
    const size_t cchCmdGreeting = strlen(pszCmdGreeting);
    wistd::unique_ptr<char[]> pszOutputBuffer = wil::make_unique_nothrow<char[]>(cchCmdGreeting + 1);

    const DWORD dwReadExpected = static_cast<DWORD>((cchCmdGreeting * sizeof(char)));
    DWORD dwReadActual = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadFile(OutPipeRead.get(), pszOutputBuffer.get(), dwReadExpected, &dwReadActual, nullptr));
    VERIFY_ARE_EQUAL(dwReadExpected, dwReadActual);
    VERIFY_ARE_EQUAL(String(pszCmdGreeting), String(pszOutputBuffer.get()));
}
