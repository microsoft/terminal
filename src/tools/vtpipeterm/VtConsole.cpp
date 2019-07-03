// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "..\..\inc\conpty.h"
#include "VtConsole.hpp"

#include <stdlib.h> /* srand, rand */
#include <time.h> /* time */

#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <wincon.h>

VtConsole::VtConsole(PipeReadCallback const pfnReadCallback,
                     bool const fHeadless,
                     bool const fUseConpty,
                     COORD const initialSize) :
    _pfnReadCallback(pfnReadCallback),
    _fHeadless(fHeadless),
    _fUseConPty(fUseConpty),
    _lastDimensions(initialSize)
{
    THROW_IF_NULL_ALLOC(pfnReadCallback);
}

void VtConsole::spawn()
{
    _spawn(L"");
}

void VtConsole::spawn(const std::wstring& command)
{
    _spawn(command);
}

// Prepares the `lpAttributeList` member of a STARTUPINFOEX for attaching a
//      client application to a pseudoconsole.
// Prior to calling this function, hPty should be initialized with a call to
//      CreatePseudoConsole, and the pAttrList should be initialized with a call
//      to InitializeProcThreadAttributeList. The caller of
//      InitializeProcThreadAttributeList should add one to the dwAttributeCount
//      param when creating the attribute list for usage by this function.
HRESULT AttachPseudoConsole(HPCON hPC, LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList)
{
    BOOL fSuccess = UpdateProcThreadAttribute(lpAttributeList,
                                              0,
                                              PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                              hPC,
                                              sizeof(HPCON),
                                              NULL,
                                              NULL);
    return fSuccess ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// Function Description:
// - Sample function which combines the creation of some basic anonymous pipes
//      and passes them to CreatePseudoConsole.
// Arguments:
// - size: The size of the conpty to create, in characters.
// - phInput: Receives the handle to the newly-created anonymous pipe for writing input to the conpty.
// - phOutput: Receives the handle to the newly-created anonymous pipe for reading the output of the conpty.
// - phPty: Receives a token value to identify this conpty
HRESULT CreatePseudoConsoleAndHandles(COORD size,
                                      _In_ DWORD dwFlags,
                                      _Out_ HANDLE* phInput,
                                      _Out_ HANDLE* phOutput,
                                      _Out_ HPCON* phPC)
{
    if (phPC == NULL || phInput == NULL || phOutput == NULL)
    {
        return E_INVALIDARG;
    }

    HANDLE outPipeOurSide;
    HANDLE inPipeOurSide;
    HANDLE outPipePseudoConsoleSide;
    HANDLE inPipePseudoConsoleSide;

    HRESULT hr = S_OK;
    if (!CreatePipe(&inPipePseudoConsoleSide, &inPipeOurSide, NULL, 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    if (SUCCEEDED(hr))
    {
        if (!CreatePipe(&outPipeOurSide, &outPipePseudoConsoleSide, NULL, 0))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        if (SUCCEEDED(hr))
        {
            hr = CreatePseudoConsole(size, inPipePseudoConsoleSide, outPipePseudoConsoleSide, dwFlags, phPC);
            if (FAILED(hr))
            {
                CloseHandle(inPipeOurSide);
                CloseHandle(outPipeOurSide);
            }
            else
            {
                *phInput = inPipeOurSide;
                *phOutput = outPipeOurSide;
            }
            CloseHandle(outPipePseudoConsoleSide);
        }
        else
        {
            CloseHandle(inPipeOurSide);
        }
        CloseHandle(inPipePseudoConsoleSide);
    }
    return hr;
}

// Method Description:
// - This version of _spawn uses the actual Pty API for creating the conhost,
//      independent of the child process. We then attach the client later.
// Arguments:
// - command: commandline of the child application to attach
// Return Value:
// - <none>
void VtConsole::_spawn(const std::wstring& command)
{
    if (_fUseConPty)
    {
        _createPseudoConsole(command);
    }
    else if (_fHeadless)
    {
        _createConptyManually(command);
    }
    else
    {
        _createConptyViaCommandline(command);
    }

    _connected = true;

    // Create our own output handling thread
    // Each console needs to make sure to drain the output from its backing host.
    _dwOutputThreadId = (DWORD)-1;
    _hOutputThread = CreateThread(nullptr,
                                  0,
                                  StaticOutputThreadProc,
                                  this,
                                  0,
                                  &_dwOutputThreadId);
}

PCWSTR GetCmdLine()
{
    return L"conhost.exe";
}

void VtConsole::_createPseudoConsole(const std::wstring& command)
{
    bool fSuccess;

    THROW_IF_FAILED(CreatePseudoConsoleAndHandles(_lastDimensions, 0, &_inPipe, &_outPipe, &_hPC));

    STARTUPINFOEX siEx;
    siEx = { 0 };
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
    size_t size;
    InitializeProcThreadAttributeList(NULL, 1, 0, (PSIZE_T)&size);
    BYTE* attrList = new BYTE[size];
    siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrList);
    fSuccess = InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, (PSIZE_T)&size);
    THROW_LAST_ERROR_IF(!fSuccess);

    THROW_IF_FAILED(AttachPseudoConsole(_hPC, siEx.lpAttributeList));

    std::wstring realCommand = command;
    if (realCommand == L"")
    {
        realCommand = L"cmd.exe";
    }

    std::unique_ptr<wchar_t[]> mutableCommandline = std::make_unique<wchar_t[]>(realCommand.length() + 1);
    THROW_IF_NULL_ALLOC(mutableCommandline);

    HRESULT hr = StringCchCopy(mutableCommandline.get(), realCommand.length() + 1, realCommand.c_str());
    THROW_IF_FAILED(hr);
    fSuccess = !!CreateProcessW(
        nullptr,
        mutableCommandline.get(),
        nullptr, // lpProcessAttributes
        nullptr, // lpThreadAttributes
        true, // bInheritHandles
        EXTENDED_STARTUPINFO_PRESENT, // dwCreationFlags
        nullptr, // lpEnvironment
        nullptr, // lpCurrentDirectory
        &siEx.StartupInfo, // lpStartupInfo
        &_piClient // lpProcessInformation
    );
    THROW_LAST_ERROR_IF(!fSuccess);
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
}

void VtConsole::_createConptyManually(const std::wstring& command)
{
    if (_fHeadless)
    {
        THROW_IF_FAILED(CreateConPty(command,
                                     _lastDimensions.X,
                                     _lastDimensions.Y,
                                     &_inPipe,
                                     &_outPipe,
                                     &_signalPipe,
                                     &_piPty));
    }
    else
    {
        _createConptyViaCommandline(command);
    }
}

void VtConsole::_createConptyViaCommandline(const std::wstring& command)
{
    std::wstring cmdline(GetCmdLine());

    if (_fHeadless)
    {
        cmdline += L" --headless";
    }

    // Create some anon pipes so we can pass handles down and into the console.
    // IMPORTANT NOTE:
    // We're creating the pipe here with un-inheritable handles, then marking
    //      the conhost sides of the pipes as inheritable. We do this because if
    //      the entire pipe is marked as inheritable, when we pass the handles
    //      to CreateProcess, at some point the entire pipe object is copied to
    //      the conhost process, which includes the terminal side of the pipes
    //      (_inPipe and _outPipe). This means that if we die, there's still
    //      outstanding handles to our side of the pipes, and those handles are
    //      in conhost, despite conhost being unable to reference those handles
    //      and close them.

    // CRITICAL: Close our side of the handles. Otherwise you'll get the same
    //      problem if you close conhost, but not us (the terminal).
    // The conhost sides of the pipe will be unique_hfile's so that they'll get
    //      closed automatically at the end of the method.
    wil::unique_hfile outPipeConhostSide;
    wil::unique_hfile inPipeConhostSide;
    wil::unique_hfile signalPipeConhostSide;

    SECURITY_ATTRIBUTES sa;
    sa = { 0 };
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    sa.lpSecurityDescriptor = nullptr;

    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&inPipeConhostSide, &_inPipe, &sa, 0));
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&_outPipe, &outPipeConhostSide, &sa, 0));

    // Mark inheritable for signal handle when creating. It'll have the same value on the other side.
    sa.bInheritHandle = TRUE;
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&signalPipeConhostSide, &_signalPipe, &sa, 0));

    THROW_IF_WIN32_BOOL_FALSE(SetHandleInformation(inPipeConhostSide.get(), HANDLE_FLAG_INHERIT, 1));
    THROW_IF_WIN32_BOOL_FALSE(SetHandleInformation(outPipeConhostSide.get(), HANDLE_FLAG_INHERIT, 1));

    STARTUPINFO si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.hStdInput = inPipeConhostSide.get();
    si.hStdOutput = outPipeConhostSide.get();
    si.hStdError = outPipeConhostSide.get();
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (!(_lastDimensions.X == 0 && _lastDimensions.Y == 0))
    {
        // STARTF_USECOUNTCHARS does not work.
        // minkernel/console/client/dllinit will write that value to conhost
        //  during init of a cmdline application, but because we're starting
        //  conhost directly, that doesn't work for us.
        std::wstringstream ss;
        ss << L" --width " << _lastDimensions.X;
        ss << L" --height " << _lastDimensions.Y;
        cmdline += ss.str();
    }

    // Attach signal handle ID onto command line using string stream for formatting
    std::wstringstream signalArg;
    signalArg << L" --signal 0x" << std::hex << HandleToUlong(signalPipeConhostSide.get());
    cmdline += signalArg.str();

    if (command.length() > 0)
    {
        cmdline += L" -- ";
        cmdline += command;
    }
    else
    {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_MINIMIZE;
    }

    bool fSuccess = !!CreateProcess(
        nullptr,
        &cmdline[0],
        nullptr, // lpProcessAttributes
        nullptr, // lpThreadAttributes
        true, // bInheritHandles
        0, // dwCreationFlags
        nullptr, // lpEnvironment
        nullptr, // lpCurrentDirectory
        &si, // lpStartupInfo
        &_piPty // lpProcessInformation
    );

    if (!fSuccess)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        std::string msg = "Failed to launch Openconsole";
        WriteFile(hOut, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
    }
}

void VtConsole::activate()
{
    _active = true;
}

void VtConsole::deactivate()
{
    _active = false;
}

DWORD WINAPI VtConsole::StaticOutputThreadProc(LPVOID lpParameter)
{
    VtConsole* const pInstance = (VtConsole*)lpParameter;
    return pInstance->_OutputThread();
}

DWORD VtConsole::_OutputThread()
{
    BYTE buffer[256];
    DWORD dwRead;
    while (true)
    {
        dwRead = 0;
        bool fSuccess = false;

        fSuccess = !!ReadFile(this->outPipe(), buffer, ARRAYSIZE(buffer), &dwRead, nullptr);
        if (!fSuccess)
        {
            HRESULT hr = GetLastError();
            exit(hr);
        }

        if (this->_active)
        {
            _pfnReadCallback(buffer, dwRead);
        }
    }
}

bool VtConsole::Repaint()
{
    std::string seq = "\x1b[7t";
    return WriteInput(seq);
}

bool VtConsole::Resize(const unsigned short rows, const unsigned short cols)
{
    if (_fUseConPty)
    {
        return SUCCEEDED(ResizePseudoConsole(_hPC, { (SHORT)cols, (SHORT)rows }));
    }
    else
    {
        return SignalResizeWindow(_signalPipe, cols, rows);
    }
}

HANDLE VtConsole::inPipe()
{
    return _inPipe;
}
HANDLE VtConsole::outPipe()
{
    return _outPipe;
}

void VtConsole::signalWindow(unsigned short sx, unsigned short sy)
{
    Resize(sy, sx);
}

bool VtConsole::WriteInput(std::string& seq)
{
    bool fSuccess = !!WriteFile(inPipe(), seq.c_str(), (DWORD)seq.length(), nullptr, nullptr);
    if (!fSuccess)
    {
        HRESULT hr = GetLastError();
        exit(hr);
    }
    return fSuccess;
}
