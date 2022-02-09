// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../winconpty.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ConPtyTests
{
    TEST_CLASS(ConPtyTests);
    const COORD defaultSize = { 80, 30 };
    TEST_METHOD(CreateConPtyNoPipes);
    TEST_METHOD(CreateConPtyBadSize);
    TEST_METHOD(GoodCreate);
    TEST_METHOD(GoodCreateMultiple);
    TEST_METHOD(SurvivesOnBreakInput);
    TEST_METHOD(SurvivesOnBreakOutput);
    TEST_METHOD(DiesOnBreakBoth);
    TEST_METHOD(DiesOnClose);
};

static HRESULT _CreatePseudoConsole(const COORD size,
                                    const HANDLE hInput,
                                    const HANDLE hOutput,
                                    const DWORD dwFlags,
                                    _Inout_ PseudoConsole* pPty)
{
    return _CreatePseudoConsole(INVALID_HANDLE_VALUE, size, hInput, hOutput, dwFlags, pPty);
}

static HRESULT AttachPseudoConsole(HPCON hPC, std::wstring& command, PROCESS_INFORMATION* ppi)
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

    const HANDLE goodIn = (HANDLE)0x4;
    const HANDLE goodOut = (HANDLE)0x8;

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
    const HANDLE goodIn = (HANDLE)0x4;
    const HANDLE goodOut = (HANDLE)0x8;
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
        _ClosePseudoConsoleMembers(&pcon);
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
        _ClosePseudoConsoleMembers(&pcon1);
    });

    VERIFY_SUCCEEDED(
        _CreatePseudoConsole(defaultSize,
                             inPipePseudoConsoleSide.get(),
                             outPipePseudoConsoleSide.get(),
                             0,
                             &pcon2));
    auto closePty2 = wil::scope_exit([&] {
        _ClosePseudoConsoleMembers(&pcon2);
    });
}

void ConPtyTests::SurvivesOnBreakInput()
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

    VERIFY_IS_TRUE(CloseHandle(inPipeOurSide.get()));

    // Wait for a couple seconds, make sure the child is still alive.
    VERIFY_ARE_EQUAL(WaitForSingleObject(pty.hConPtyProcess, 2000), (DWORD)WAIT_TIMEOUT);
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
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

    VERIFY_IS_TRUE(CloseHandle(outPipeOurSide.get()));

    // Wait for a couple seconds, make sure the child is still alive.
    VERIFY_ARE_EQUAL(WaitForSingleObject(pty.hConPtyProcess, 2000), (DWORD)WAIT_TIMEOUT);
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
}

void ConPtyTests::DiesOnBreakBoth()
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

    // Close one of the pipes...
    VERIFY_IS_TRUE(CloseHandle(outPipeOurSide.get()));

    // ... Wait for a couple seconds, make sure the child is still alive.
    VERIFY_ARE_EQUAL(WaitForSingleObject(pty.hConPtyProcess, 2000), (DWORD)WAIT_TIMEOUT);
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_EQUAL(dwExit, (DWORD)STILL_ACTIVE);

    // Tricky - write some input to the pcon. We need to do this so conhost can
    //      realize that the output pipe has broken.
    VERIFY_SUCCEEDED(WriteFile(inPipeOurSide.get(), L"a", sizeof(wchar_t), nullptr, nullptr));

    // Close the other pipe, and make sure conhost dies
    VERIFY_IS_TRUE(CloseHandle(inPipeOurSide.get()));

    VERIFY_ARE_EQUAL(WaitForSingleObject(pty.hConPtyProcess, 10000), (DWORD)WAIT_OBJECT_0);
    VERIFY_IS_TRUE(GetExitCodeProcess(pty.hConPtyProcess, &dwExit));
    VERIFY_ARE_NOT_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
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

    GetExitCodeProcess(hConPtyProcess.get(), &dwExit);
    VERIFY_ARE_NOT_EQUAL(dwExit, (DWORD)STILL_ACTIVE);
}
