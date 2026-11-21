// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <conpty-static.h>
#include <ForegroundBoost.h>

#include "../winconpty.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using unique_hpcon = wil::unique_any<HPCON, decltype(&::ClosePseudoConsole), ::ClosePseudoConsole>;

struct InOut
{
    wil::unique_handle in;
    wil::unique_handle out;
};

struct Pipes
{
    InOut our;
    InOut conpty;
};

static Pipes createPipes()
{
    Pipes p;
    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(sa),
        .bInheritHandle = TRUE,
    };

    VERIFY_IS_TRUE(CreatePipe(p.conpty.in.addressof(), p.our.in.addressof(), &sa, 0));
    VERIFY_IS_TRUE(CreatePipe(p.our.out.addressof(), p.conpty.out.addressof(), &sa, 0));
    VERIFY_IS_TRUE(SetHandleInformation(p.our.in.get(), HANDLE_FLAG_INHERIT, 0));
    VERIFY_IS_TRUE(SetHandleInformation(p.our.out.get(), HANDLE_FLAG_INHERIT, 0));

    // ConPTY requests a DA1 report on startup. Emulate the response from the terminal.
    DWORD written{};
    WriteFile(p.our.in.get(), "\x1b[?61c", 6, &written, nullptr);

    return p;
}

struct PTY
{
    unique_hpcon hpcon;
    InOut pipes;
};

static PTY createPseudoConsole()
{
    auto pipes = createPipes();
    unique_hpcon hpcon;
    VERIFY_SUCCEEDED(ConptyCreatePseudoConsole({ 80, 30 }, pipes.conpty.in.get(), pipes.conpty.out.get(), 0, hpcon.addressof()));
    return {
        .hpcon = std::move(hpcon),
        .pipes = std::move(pipes.our),
    };
}

static std::string readOutputToEOF(const InOut& io)
{
    std::string accumulator;
    char buffer[1024];

    for (;;)
    {
        DWORD read;
        const auto ok = ReadFile(io.out.get(), &buffer[0], sizeof(buffer), &read, nullptr);

        if (!ok)
        {
            const auto lastError = GetLastError();
            if (lastError == ERROR_BROKEN_PIPE)
            {
                break;
            }
            else
            {
                VERIFY_WIN32_BOOL_SUCCEEDED(false);
            }
        }

        accumulator.append(&buffer[0], read);
    }

    return accumulator;
}

class ConPtyTests
{
    BEGIN_TEST_CLASS(ConPtyTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:15") // 15s timeout
    END_TEST_CLASS()

    const COORD defaultSize = { 80, 30 };
    TEST_METHOD(CreateConPtyNoPipes);
    TEST_METHOD(CreateConPtyBadSize);
    TEST_METHOD(GoodCreate);
    TEST_METHOD(GoodCreateMultiple);
    TEST_METHOD(SurvivesOnBreakOutput);
    TEST_METHOD(DiesOnClose);
    TEST_METHOD(ReleasePseudoConsole);
};

static HRESULT _CreatePseudoConsole(const COORD size,
                                    const HANDLE hInput,
                                    const HANDLE hOutput,
                                    const DWORD dwFlags,
                                    _Inout_ PseudoConsole* pPty)
{
    return _CreatePseudoConsole(INVALID_HANDLE_VALUE, size, hInput, hOutput, dwFlags, pPty);
}

static HRESULT AttachPseudoConsole(HPCON hPC, std::wstring command, PROCESS_INFORMATION* ppi)
{
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    RETURN_LAST_ERROR_IF(size == 0);

    const auto buffer = std::make_unique<std::byte[]>(gsl::narrow_cast<size_t>(size));

    STARTUPINFOEXW siEx{};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES; // we will leave the handles empty to ensure none are inherited.
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.get());

    RETURN_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size));
    auto deleteAttrList = wil::scope_exit([&] {
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
    });

    RETURN_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HANDLE), nullptr, nullptr));

    RETURN_IF_WIN32_BOOL_FALSE(CreateProcessW(
        nullptr,
        command.data(),
        nullptr, // lpProcessAttributes
        nullptr, // lpThreadAttributes
        true, // bInheritHandles
        EXTENDED_STARTUPINFO_PRESENT, // dwCreationFlags
        nullptr, // lpEnvironment
        nullptr, // lpCurrentDirectory
        &siEx.StartupInfo, // lpStartupInfo
        ppi // lpProcessInformation
        ));

    return S_OK;
}

void ConPtyTests::CreateConPtyNoPipes()
{
    PseudoConsole pcon{};

    const auto goodIn = (HANDLE)0x4;
    const auto goodOut = (HANDLE)0x8;

    // We only need one of the two handles to start successfully. However,
    // INVALID_HANDLE for either will be rejected by CreateProcess, but nullptr
    //      will be acceptable.
    // So make sure INVALID_HANDLE always fails, and nullptr succeeds as long as one is real.
    VERIFY_FAILED(_CreatePseudoConsole(defaultSize, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, 0, &pcon));
    VERIFY_FAILED(_CreatePseudoConsole(defaultSize, INVALID_HANDLE_VALUE, goodOut, 0, &pcon));
    VERIFY_FAILED(_CreatePseudoConsole(defaultSize, goodIn, INVALID_HANDLE_VALUE, 0, &pcon));

    VERIFY_FAILED(_CreatePseudoConsole(defaultSize, nullptr, nullptr, 0, &pcon));

    VERIFY_SUCCEEDED(_CreatePseudoConsole(defaultSize, nullptr, goodOut, 0, &pcon));
    _ClosePseudoConsoleMembers(&pcon);

    VERIFY_SUCCEEDED(_CreatePseudoConsole(defaultSize, goodIn, nullptr, 0, &pcon));
    _ClosePseudoConsoleMembers(&pcon);
}

void ConPtyTests::CreateConPtyBadSize()
{
    PseudoConsole pcon{};
    COORD badSize = { 0, 0 };
    const auto goodIn = (HANDLE)0x4;
    const auto goodOut = (HANDLE)0x8;
    VERIFY_FAILED(_CreatePseudoConsole(badSize, goodIn, goodOut, 0, &pcon));

    badSize = { 0, defaultSize.Y };
    VERIFY_FAILED(_CreatePseudoConsole(badSize, goodIn, goodOut, 0, &pcon));

    badSize = { defaultSize.X, 0 };
    VERIFY_FAILED(_CreatePseudoConsole(badSize, goodIn, goodOut, 0, &pcon));
}

void ConPtyTests::GoodCreate()
{
    PseudoConsole pcon{};
    auto pipes = createPipes();

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             pipes.conpty.in.get(),
                             pipes.conpty.out.get(),
                             0,
                             &pcon));

    auto closePty = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon);
    });
}

void ConPtyTests::GoodCreateMultiple()
{
    PseudoConsole pcon1{};
    PseudoConsole pcon2{};
    auto pipes = createPipes();

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             pipes.conpty.in.get(),
                             pipes.conpty.out.get(),
                             0,
                             &pcon1));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon1);
    });

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             pipes.conpty.in.get(),
                             pipes.conpty.out.get(),
                             0,
                             &pcon2));
    auto closePty2 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon2);
    });
}

void ConPtyTests::SurvivesOnBreakOutput()
{
    PseudoConsole pty = { 0 };
    auto pipes = createPipes();

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             pipes.conpty.in.get(),
                             pipes.conpty.out.get(),
                             0,
                             &pty));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pty);
    });

    DWORD dwExit;
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    wil::unique_process_information piClient;
    std::wstring realCommand = L"cmd.exe";
    VERIFY_SUCCEEDED(AttachPseudoConsole(&pty, realCommand, piClient.addressof()));

    VERIFY_IS_TRUE(GetExitCodeProcess(piClient.hProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    pipes.our.out.reset();

    // Wait for a couple seconds, make sure the child is still alive.
    VERIFY_ARE_EQUAL(WaitForSingleObject(pty.hConPtyProcess, 2000), (DWORD)WAIT_TIMEOUT);
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
}

void ConPtyTests::DiesOnClose()
{
    BEGIN_TEST_METHOD_PROPERTIES()
        TEST_METHOD_PROPERTY(L"Data:commandline",
                             L"{"
                             // TODO: MSFT:20146938 - investigate and possibly re-enable this case
                             // L"cmd.exe /c dir,"
                             L"ping localhost,"
                             L"cmd.exe /c echo Hello World,"
                             L"cmd.exe /c for /L %i () DO echo Hello World %i,"
                             L"cmd.exe"
                             L"}")
    END_TEST_METHOD_PROPERTIES();
    String testCommandline;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"commandline", testCommandline), L"Get a commandline to test");

    PseudoConsole pty = { 0 };
    auto pipes = createPipes();

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             pipes.conpty.in.get(),
                             pipes.conpty.out.get(),
                             0,
                             &pty));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pty);
    });

    DWORD dwExit;
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    wil::unique_process_information piClient;
    std::wstring realCommand(reinterpret_cast<const wchar_t*>(testCommandline.GetBuffer()), testCommandline.GetLength());
    VERIFY_SUCCEEDED(AttachPseudoConsole(&pty, realCommand, piClient.addressof()));

    VERIFY_IS_TRUE(GetExitCodeProcess(piClient.hProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    // Duplicate the pty process, it'll get closed and zero'd after the call to close
    wil::unique_handle hConPtyProcess{};
    THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), pty.hConPtyProcess, GetCurrentProcess(), hConPtyProcess.put(), 0, FALSE, DUPLICATE_SAME_ACCESS));

    VERIFY_IS_TRUE(GetExitCodeProcess(hConPtyProcess.get(), &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    Log::Comment(NoThrowString().Format(L"Sleep a bit to let the process attach"));
    Sleep(100);

    _ClosePseudoConsoleMembers(&pty);

    WaitForSingleObject(hConPtyProcess.get(), 3000);
    GetExitCodeProcess(hConPtyProcess.get(), &dwExit);
    VERIFY_ARE_NOT_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
}

// Issues with ConptyReleasePseudoConsole functionality might present itself as sporadic/flaky test failures,
// which should not ever happen (otherwise something is broken). This is because "start /b" runs concurrently
// with the initially spawned "cmd.exe" exiting and so this test involves sort of a race condition.
void ConPtyTests::ReleasePseudoConsole()
{
    const auto pty = createPseudoConsole();
    wil::unique_process_information pi;
    VERIFY_SUCCEEDED(AttachPseudoConsole(pty.hpcon.get(), L"cmd.exe /c start /b cmd.exe /c echo foobar", pi.addressof()));
    VERIFY_SUCCEEDED(ConptyReleasePseudoConsole(pty.hpcon.get()));

    const auto output = readOutputToEOF(pty.pipes);
    VERIFY_ARE_NOT_EQUAL(std::string::npos, output.find("foobar"));
}

class ForegroundBoostTests
{
    BEGIN_TEST_CLASS(ForegroundBoostTests)
        TEST_CLASS_PROPERTY(L"TestTimeout", L"0:0:30")
    END_TEST_CLASS()

    TEST_METHOD(OutputOnlyClientIsPublishedBeforeInput)
    {
        Run(false, true);
    }

    TEST_METHOD(DisconnectRemovesLivingClient)
    {
        Run(true, true);
    }

    TEST_METHOD(UnconfiguredChannelDoesNotPublishClients)
    {
        Run(false, false);
    }

    static void Send(const PTY& pty, const char* text, DWORD length)
    {
        DWORD written{};
        VERIFY_IS_TRUE(WriteFile(pty.pipes.in.get(), text, length, &written, nullptr));
        VERIFY_ARE_EQUAL(length, written);
    }

    struct Channel
    {
        wil::unique_handle section;
        wil::unique_mapview_ptr<Microsoft::Console::ForegroundBoostSharedState> state;
        wil::unique_event changed;
        wil::unique_handle host;

        Channel(HPCON hpcon, bool enabled)
        {
            using namespace Microsoft::Console;
            section.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ForegroundBoostSharedState), nullptr));
            VERIFY_IS_NOT_NULL(section.get());
            state.reset(static_cast<ForegroundBoostSharedState*>(
                MapViewOfFile(section.get(), FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(ForegroundBoostSharedState))));
            VERIFY_IS_NOT_NULL(state.get());
            new (state.get()) ForegroundBoostSharedState{};
            changed.create(wil::EventOptions::None);
            if (enabled)
            {
                VERIFY_SUCCEEDED(ConptyConfigureForegroundBoost(hpcon, section.get(), changed.get(), host.addressof()));
                VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(changed.get(), 5000));
            }
        }

        void WaitForClient(DWORD pid)
        {
            if (pid)
            {
                WaitForClients({ pid });
            }
            else
            {
                WaitForClients({});
            }
        }

        void WaitForClients(std::initializer_list<DWORD> processIds)
        {
            const auto deadline = GetTickCount64() + 5000;
            for (;;)
            {
                Microsoft::Console::ForegroundBoostSnapshot snapshot;
                const auto hr = state->Read(snapshot);
                if (hr != HRESULT_FROM_WIN32(ERROR_RETRY))
                {
                    VERIFY_SUCCEEDED(hr);
                    if (static_cast<size_t>(snapshot.count) == processIds.size() &&
                        std::ranges::all_of(processIds, [&snapshot](const auto pid) {
                            return std::ranges::any_of(snapshot.clients, [pid](const auto& client) { return client.processId == pid; });
                        }))
                    {
                        return;
                    }
                }
                const auto now = GetTickCount64();
                THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), now >= deadline);
                VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(changed.get(), gsl::narrow_cast<DWORD>(deadline - now)));
            }
        }
    };

    static void WaitForOutput(const PTY& pty, HANDLE client, std::string& output, const char* marker)
    {
        const auto deadline = GetTickCount64() + 8000;
        while (output.find(marker) == std::string::npos)
        {
            THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), GetTickCount64() >= deadline);
            DWORD available{}, read{};
            THROW_IF_WIN32_BOOL_FALSE(PeekNamedPipe(pty.pipes.out.get(), nullptr, 0, nullptr, &available, nullptr));
            if (available)
            {
                std::array<char, 1024> buffer{};
                THROW_IF_WIN32_BOOL_FALSE(ReadFile(pty.pipes.out.get(), buffer.data(), (std::min)(available, DWORD{ 1024 }), &read, nullptr));
                output.append(buffer.data(), read);
            }
            else
            {
                THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_PROCESS_ABORTED), WaitForSingleObject(client, 10) != WAIT_TIMEOUT);
            }
        }
    }

    TEST_METHOD(ActualApiRegistersAndClearsWithoutChangingClass)
    {
        const auto pty = createPseudoConsole();
        const auto host = reinterpret_cast<PseudoConsole*>(pty.hpcon.get())->hConPtyProcess;
        const auto before = GetPriorityClass(host);
        VERIFY_ARE_NOT_EQUAL(0ul, before);
        const auto window = CreateWindowExW(0, L"STATIC", L"Foreground boost API test", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        VERIFY_IS_NOT_NULL(window);
        const auto destroy = wil::scope_exit([&] { DestroyWindow(window); });
        HANDLE processes[]{ host };
        VERIFY_SUCCEEDED(Microsoft::Console::SetForegroundBoostProcesses(window, 1, processes));
        VERIFY_ARE_EQUAL(before, GetPriorityClass(host));
        VERIFY_SUCCEEDED(Microsoft::Console::SetForegroundBoostProcesses(window, 0, nullptr));
        VERIFY_ARE_EQUAL(before, GetPriorityClass(host));
    }

    TEST_METHOD(ExistingAndNewClientsArePublishedWithoutInput)
    {
        const auto pty = createPseudoConsole();
        std::array<wil::unique_process_information, 2> clients;
        const auto terminate = wil::scope_exit([&] {
            for (const auto& client : clients)
            {
                if (client.hProcess && WaitForSingleObject(client.hProcess, 0) == WAIT_TIMEOUT)
                {
                    LOG_IF_WIN32_BOOL_FALSE(TerminateProcess(client.hProcess, ERROR_CANCELLED));
                    LOG_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), WaitForSingleObject(client.hProcess, 5000) != WAIT_OBJECT_0);
                }
            }
        });
        const std::wstring command = L"powershell.exe -NoProfile -NonInteractive -Command \"[Console]::WriteLine('FOREGROUND_CLIENT_READY'); Start-Sleep -Seconds 30\"";
        VERIFY_SUCCEEDED(AttachPseudoConsole(pty.hpcon.get(), command, clients[0].addressof()));
        std::string output;
        WaitForOutput(pty, clients[0].hProcess, output, "FOREGROUND_CLIENT_READY");
        Channel channel{ pty.hpcon.get(), true };
        channel.WaitForClients({ clients[0].dwProcessId });

        output.clear();
        VERIFY_SUCCEEDED(AttachPseudoConsole(pty.hpcon.get(), command, clients[1].addressof()));
        WaitForOutput(pty, clients[1].hProcess, output, "FOREGROUND_CLIENT_READY");
        channel.WaitForClients({ clients[0].dwProcessId, clients[1].dwProcessId });

        VERIFY_IS_TRUE(TerminateProcess(clients[1].hProcess, ERROR_CANCELLED));
        VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(clients[1].hProcess, 5000));
        channel.WaitForClients({ clients[0].dwProcessId });
    }

    static void Run(bool disconnect, bool enabled)
    {
        const auto pty = createPseudoConsole();
        Channel channel{ pty.hpcon.get(), enabled };
        const auto host = reinterpret_cast<PseudoConsole*>(pty.hpcon.get())->hConPtyProcess;
        wil::unique_event disconnectEvent;
        const auto disconnectName = L"Local\\OpenConsoleForegroundDisconnect-" + std::to_wstring(GetCurrentProcessId());
        std::wstring command = L"powershell.exe -NoProfile -NonInteractive -Command \"[Console]::WriteLine('FOREGROUND_CLIENT_READY'); Start-Sleep -Seconds 30\"";
        if (disconnect)
        {
            disconnectEvent.create(wil::EventOptions::ManualReset, disconnectName.c_str());
            command = LR"PS(powershell.exe -NoProfile -NonInteractive -Command "Add-Type -TypeDefinition '
using System.Runtime.InteropServices;
public static class ForegroundBoostTestClient {
    [DllImport(\"kernel32.dll\")] public static extern bool FreeConsole();
}
'; [Console]::WriteLine('FOREGROUND_CLIENT_READY');
[void][System.Threading.EventWaitHandle]::OpenExisting(')PS" +
                      disconnectName +
                      LR"PS(').WaitOne();
if (-not [ForegroundBoostTestClient]::FreeConsole()) { exit 1 };
Start-Sleep -Seconds 30")PS";
        }
        wil::unique_process_information client;
        const auto terminate = wil::scope_exit([&] {
            if (client.hProcess && WaitForSingleObject(client.hProcess, 0) == WAIT_TIMEOUT)
            {
                LOG_IF_WIN32_BOOL_FALSE(TerminateProcess(client.hProcess, ERROR_CANCELLED));
                LOG_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), WaitForSingleObject(client.hProcess, 5000) != WAIT_OBJECT_0);
            }
        });
        VERIFY_SUCCEEDED(AttachPseudoConsole(pty.hpcon.get(), command, client.addressof()));
        std::string output;
        WaitForOutput(pty, client.hProcess, output, "FOREGROUND_CLIENT_READY");
        channel.WaitForClient(enabled ? client.dwProcessId : 0);
        VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(host));
        VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(client.hProcess));
        if (enabled)
        {
            Microsoft::Console::ForegroundBoostSnapshot snapshot;
            VERIFY_SUCCEEDED(channel.state->Read(snapshot));
            FILETIME creation{}, exit{}, kernel{}, user{};
            VERIFY_IS_TRUE(GetProcessTimes(client.hProcess, &creation, &exit, &kernel, &user));
            VERIFY_ARE_EQUAL(0l, CompareFileTime(&creation, &snapshot.clients[0].creationTime));
        }
        if (disconnect)
        {
            VERIFY_IS_TRUE(SetEvent(disconnectEvent.get()));
            channel.WaitForClient(0);
            VERIFY_ARE_EQUAL(DWORD{ WAIT_TIMEOUT }, WaitForSingleObject(client.hProcess, 0));
        }
        for (size_t cycle = 0; cycle < 2; ++cycle)
        {
            if (enabled)
            {
                VERIFY_IS_TRUE(ResetEvent(channel.changed.get()));
            }
            Send(pty, "\x1b[O", 3);
            if (enabled)
            {
                VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(channel.changed.get(), 5000));
            }
            channel.WaitForClient(enabled && !disconnect ? client.dwProcessId : 0);
            VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(host));
            VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(client.hProcess));
            if (enabled)
            {
                VERIFY_IS_TRUE(ResetEvent(channel.changed.get()));
            }
            Send(pty, "\x1b[I", 3);
            if (enabled)
            {
                VERIFY_ARE_EQUAL(DWORD{ WAIT_OBJECT_0 }, WaitForSingleObject(channel.changed.get(), 5000));
            }
            channel.WaitForClient(enabled && !disconnect ? client.dwProcessId : 0);
            VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(host));
            VERIFY_ARE_EQUAL(DWORD{ NORMAL_PRIORITY_CLASS }, GetPriorityClass(client.hProcess));
        }
    }
};
