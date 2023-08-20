// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <conpty-static.h>

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
    _ClosePseudoConsoleMembers(&pcon, INFINITE);

    VERIFY_SUCCEEDED(_CreatePseudoConsole(defaultSize, goodIn, nullptr, 0, &pcon));
    _ClosePseudoConsoleMembers(&pcon, INFINITE);
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
    wil::unique_handle outPipeOurSide;
    wil::unique_handle inPipeOurSide;
    wil::unique_handle outPipePseudoConsoleSide;
    wil::unique_handle inPipePseudoConsoleSide;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    VERIFY_IS_TRUE(CreatePipe(inPipePseudoConsoleSide.addressof(), inPipeOurSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(CreatePipe(outPipeOurSide.addressof(), outPipePseudoConsoleSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(SetHandleInformation(inPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));
    VERIFY_IS_TRUE(SetHandleInformation(outPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pcon));

    auto closePty = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon, INFINITE);
    });
}

void ConPtyTests::GoodCreateMultiple()
{
    PseudoConsole pcon1{};
    PseudoConsole pcon2{};
    wil::unique_handle outPipeOurSide;
    wil::unique_handle inPipeOurSide;
    wil::unique_handle outPipePseudoConsoleSide;
    wil::unique_handle inPipePseudoConsoleSide;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    VERIFY_IS_TRUE(CreatePipe(inPipePseudoConsoleSide.addressof(), inPipeOurSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(CreatePipe(outPipeOurSide.addressof(), outPipePseudoConsoleSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(SetHandleInformation(inPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));
    VERIFY_IS_TRUE(SetHandleInformation(outPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pcon1));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon1, INFINITE);
    });

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pcon2));
    auto closePty2 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon2, INFINITE);
    });
}

void ConPtyTests::SurvivesOnBreakOutput()
{
    PseudoConsole pty = { 0 };
    wil::unique_handle outPipeOurSide;
    wil::unique_handle inPipeOurSide;
    wil::unique_handle outPipePseudoConsoleSide;
    wil::unique_handle inPipePseudoConsoleSide;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    VERIFY_IS_TRUE(CreatePipe(inPipePseudoConsoleSide.addressof(), inPipeOurSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(CreatePipe(outPipeOurSide.addressof(), outPipePseudoConsoleSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(SetHandleInformation(inPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));
    VERIFY_IS_TRUE(SetHandleInformation(outPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pty));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pty, INFINITE);
    });

    DWORD dwExit;
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    wil::unique_process_information piClient;
    std::wstring realCommand = L"cmd.exe";
    VERIFY_SUCCEEDED(AttachPseudoConsole(&pty, realCommand, piClient.addressof()));

    VERIFY_IS_TRUE(GetExitCodeProcess(piClient.hProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    VERIFY_IS_TRUE(CloseHandle(outPipeOurSide.get()));

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
    wil::unique_handle outPipeOurSide;
    wil::unique_handle inPipeOurSide;
    wil::unique_handle outPipePseudoConsoleSide;
    wil::unique_handle inPipePseudoConsoleSide;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    VERIFY_IS_TRUE(CreatePipe(inPipePseudoConsoleSide.addressof(), inPipeOurSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(CreatePipe(outPipeOurSide.addressof(), outPipePseudoConsoleSide.addressof(), &sa, 0));
    VERIFY_IS_TRUE(SetHandleInformation(inPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));
    VERIFY_IS_TRUE(SetHandleInformation(outPipeOurSide.get(), HANDLE_FLAG_INHERIT, 0));

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pty));
    auto closePty1 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pty, INFINITE);
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

    _ClosePseudoConsoleMembers(&pty, INFINITE);

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
