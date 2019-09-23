// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ConptyConnection.h"

#include <Windows.h>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConptyConnection::ConptyConnection(hstring const& commandline,
                                       uint32_t initialRows,
                                       uint32_t initialCols) :
        _connected{ false },
        _inPipe{ INVALID_HANDLE_VALUE },
        _outPipe{ INVALID_HANDLE_VALUE },
        _hPC{ INVALID_HANDLE_VALUE },
        _outputThreadId{ 0 },
        _hOutputThread{ INVALID_HANDLE_VALUE },
        _piClient{ 0 }
    {
        _commandline = commandline;
        _initialRows = initialRows;
        _initialCols = initialCols;
    }

    winrt::event_token ConptyConnection::TerminalOutput(TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        return _outputHandlers.add(handler);
    }

    void ConptyConnection::TerminalOutput(winrt::event_token const& token) noexcept
    {
        _outputHandlers.remove(token);
    }

    winrt::event_token ConptyConnection::TerminalDisconnected(TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        handler;

        throw hresult_not_implemented();
    }

    void ConptyConnection::TerminalDisconnected(winrt::event_token const& token) noexcept
    {
        token;

        // throw hresult_not_implemented();
    }

    void ConptyConnection::Start()
    {
        _CreatePseudoConsole();

        _connected = true;

        // Create our own output handling thread
        // Each console needs to make sure to drain the output from it's backing host.
        _outputThreadId = (DWORD)-1;
        _hOutputThread = CreateThread(nullptr,
                                      0,
                                      StaticOutputThreadProc,
                                      this,
                                      0,
                                      &_outputThreadId);

        //// When we recieve some data:
        //hstring outputFromConpty = L"hello world";
        ////TerminalOutputEventArgs args(outputFromConpty);
        ////_outputHandlers(*this, args);
        //_outputHandlers(outputFromConpty);
    }

    void ConptyConnection::WriteInput(hstring const& data)
    {
        data;

        throw hresult_not_implemented();
    }

    void ConptyConnection::Resize(uint32_t rows, uint32_t columns)
    {
        rows;
        columns;

        throw hresult_not_implemented();
    }

    void ConptyConnection::Close()
    {
        // TODO:
        //      terminate the output thread
        //      Close our handles
        //      Close the Pseudoconsole
        //      terminate our processes
        throw hresult_not_implemented();
    }

    // Function Description:
    // - Sample function which combines the creation of some basic anonymous pipes
    //      and passes them to CreatePseudoConsole.
    // Arguments:
    // - size: The size of the conpty to create, in characters.
    // - phInput: Receives the handle to the newly-created anonymous pipe for writing input to the conpty.
    // - phOutput: Receives the handle to the newly-created anonymous pipe for reading the output of the conpty.
    // - phPty: Receives a token value to identify this conpty
    HRESULT _CreatePseudoConsoleAndHandles(COORD size,
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

    // Prepares the `lpAttributeList` member of a STARTUPINFOEX for attaching a
    //      client application to a pseudoconsole.
    // Prior to calling this function, hPty should be initialized with a call to
    //      CreatePseudoConsole, and the pAttrList should be initialized with a call
    //      to InitializeProcThreadAttributeList. The caller of
    //      InitializeProcThreadAttributeList should add one to the dwAttributeCount
    //      param when creating the attribute list for usage by this function.
    HRESULT _AttachPseudoConsole(HPCON hPC, LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList)
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

    void ConptyConnection::_CreatePseudoConsole()
    {
        bool fSuccess;

        COORD dimensions{ _initialCols, _initialRows };
        THROW_IF_FAILED(_CreatePseudoConsoleAndHandles(dimensions, 0, &_inPipe, &_outPipe, &_hPC));

        STARTUPINFOEX siEx;
        siEx = { 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        size_t size;
        InitializeProcThreadAttributeList(NULL, 1, 0, (PSIZE_T)&size);
        BYTE* attrList = new BYTE[size];
        siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrList);
        fSuccess = InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, (PSIZE_T)&size);
        THROW_LAST_ERROR_IF(!fSuccess);

        THROW_IF_FAILED(_AttachPseudoConsole(_hPC, siEx.lpAttributeList));

        std::wstring realCommand = _commandline.c_str(); //winrt::to_string(_commandline);
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

    DWORD WINAPI ConptyConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        ConptyConnection* const pInstance = (ConptyConnection*)lpParameter;
        return pInstance->_OutputThread();
    }

    DWORD ConptyConnection::_OutputThread()
    {
        BYTE buffer[256];
        DWORD dwRead;
        while (true)
        {
            dwRead = 0;
            bool fSuccess = false;

            fSuccess = !!ReadFile(_outPipe, buffer, ARRAYSIZE(buffer), &dwRead, nullptr);

            THROW_LAST_ERROR_IF(!fSuccess);

            // Convert buffer to hstring
            char* pchStr = (char*)(buffer);
            std::string str{ pchStr, dwRead };
            auto hstr = winrt::to_hstring(str);

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);

            // if (this->_active)
            // {
            //     _pfnReadCallback(buffer, dwRead);
            // }
        }
    }
}
