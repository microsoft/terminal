// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <til/u8u16convert.h>
#include <ranges>
#include <fmt/ranges.h>

#include "../ConsoleArguments.hpp"
#include "../srvinit.h"
#include "../_stream.h"
#include "../../interactivity/inc/ServiceLocator.hpp"
#include "../../server/IoThread.h"

#include "conmsg.h"

struct NullDeviceComm : public IDeviceComm
{
    static constexpr DWORD InitialConnectMessage = 0xA0000000;
    static constexpr DWORD EnqueuedReadInputMessage = 0xAA000000;
    mutable BOOL enqueueInput{ TRUE };

    void _RetireIoOperation(CD_IO_COMPLETE* const complete) const
    {
        if (complete->Identifier.LowPart == InitialConnectMessage)
        {
            memcpy((void*)&Connection, reinterpret_cast<uint8_t*>(complete->Write.Data) + complete->Write.Offset, std::min(sizeof(Connection), (unsigned long long)complete->Write.Size));
        }
        fmt::print(L"OP-{} Retired\n", complete->Identifier.LowPart);
    }

    HRESULT SetServerInformation(CD_IO_SERVER_INFORMATION* const) const override
    {
        return S_FALSE;
    }
    HRESULT ReadIo(PCONSOLE_API_MSG pReplyMessage, CONSOLE_API_MSG* const pMessage) const override
    {
        if (pReplyMessage)
        {
            fmt::print(L"OP-{} Reply Received During Read\n", pReplyMessage->Complete.Identifier.LowPart);
            _RetireIoOperation(&pReplyMessage->Complete);
        }

        CONSOLE_API_MSG local{};
        if (!Connection.Process)
        {
            local.Descriptor.Identifier.LowPart = InitialConnectMessage;
            local.Descriptor.Function = CONSOLE_IO_CONNECT;
            local.Descriptor.Process = GetCurrentProcessId(); // CAC - Process=PID
            local.Descriptor.Object = GetCurrentThreadId(); // CAC - Object=TID
            local.Descriptor.InputSize = sizeof(CONSOLE_SERVER_MSG);
            goto send;
        }

        static DWORD seq = 1;
        local.Descriptor.Process = Connection.Process;

        switch (seq)
        {
        case 1:
        {
            local.Descriptor.Object = Connection.Output;
            auto& modeMsg = PrepareConsoleMessage<ConsolepSetMode>(local);
            modeMsg.Mode = 0x3 | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            break;
        }
        case 2:
        {
            local.Descriptor.Object = Connection.Input;
            auto& msg = PrepareConsoleMessage<ConsolepSetMode>(local);
            msg.Mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
            break;
        }
        case 3:
        case 4:
        {
            local.Descriptor.Identifier.LowPart = seq;
            local.Descriptor.Function = CONSOLE_IO_RAW_WRITE;
            local.Descriptor.Object = Connection.Output;
            local.Descriptor.InputSize = 6;
            break;
        }
        case 5:
        {
            local.Descriptor.Object = Connection.Output;
            auto& msg = PrepareConsoleMessage<ConsolepFillConsoleOutput>(local);
            msg = {
                .WriteCoord = { 0, 0 },
                .ElementType = CONSOLE_ATTRIBUTE,
                .Element = 0x43,
                .Length = 40,
            };
            break;
        }
        default:
        {
            BOOL old{ FALSE };
            while (!enqueueInput)
                WaitOnAddress(&enqueueInput, &old, sizeof(enqueueInput), INFINITE);
            if (enqueueInput)
            {
                local.Descriptor.Identifier.LowPart = EnqueuedReadInputMessage;
                auto& msg = PrepareConsoleMessage<ConsolepGetConsoleInput>(local);
                msg.Unicode = TRUE;
                //local.Descriptor.Function = CONSOLE_IO_RAW_READ;
                local.Descriptor.Object = Connection.Input;
                //local.Descriptor.Process = Connection.Process;
                //local.Descriptor.InputSize = 0;
                local.Descriptor.OutputSize = 1024;
                enqueueInput = FALSE;
            }
            break;
        }
        }

    send:
        seq++;
        memcpy(&pMessage->Descriptor, &local.Descriptor, sizeof(CONSOLE_API_MSG) - FIELD_OFFSET(CONSOLE_API_MSG, Descriptor));
        // The easiest way to get the IO thread to stop reading from us us to simply
        // suspend it. The fuzzer doesn't need a device IO thread.
        return S_OK;
    }
    HRESULT CompleteIo(CD_IO_COMPLETE* const completion) const override
    {
        fmt::print(L"OP-{} Completed\n", completion->Identifier.LowPart);
        _RetireIoOperation(completion);
        return S_OK;
    }
    HRESULT ReadInput(CD_IO_OPERATION* const operation) const override
    {
        fmt::print(L"OP-{} ReadInput\n", operation->Identifier.LowPart);
        if (operation->Identifier.LowPart == InitialConnectMessage)
        {
            CONSOLE_SERVER_MSG msg{
                .StartupFlags = STARTF_USECOUNTCHARS,
                .ShowWindow = SW_NORMAL,
                .ScreenBufferSize = { 80, 25 },
                .WindowSize = { 80, 25 },
                .ProcessGroupId = 0xFFFFFFFF,
                .ConsoleApp = TRUE,
                .WindowVisible = TRUE,
                .TitleLength = 15 * 2,
                .Title = L"Fuzzing Harness",
                .ApplicationNameLength = 8 * 2,
                .ApplicationName = L"fuzz.exe",
                .CurrentDirectoryLength = 3 * 2,
                .CurrentDirectory = L"C:\\",
            };
            memcpy((uint8_t*)(operation->Buffer.Data) + operation->Buffer.Offset, &msg, std::min((unsigned long long)operation->Buffer.Size, sizeof(msg)));
            return S_OK;
        }

        switch (operation->Identifier.LowPart)
        {
        case 3:
        case 4:
            memcpy((uint8_t*)(operation->Buffer.Data) + operation->Buffer.Offset, "hello\n", std::min(operation->Buffer.Size, 6UL));
            return S_OK;
        default:
            fmt::print(L"Unexpected ReadInput for IO OP {}\n", operation->Identifier.LowPart);
            break;
        }
        //SuspendThread(GetCurrentThread());
        return S_FALSE;
    }
    HRESULT WriteOutput(CD_IO_OPERATION* const operation) const override
    {
        fmt::print(L"OP-{} WriteOutput\n", operation->Identifier.LowPart);
        fmt::print(L"Writing output of size {}\n", operation->Buffer.Size);
        if (operation->Identifier.LowPart == EnqueuedReadInputMessage)
        {
            // TODO(DH) - figure out why Offset should be ignored here. It looks like that's how CompleteIo gets the _message_ back, but it's unused here?
            auto records = reinterpret_cast<INPUT_RECORD*>(reinterpret_cast<uint8_t*>(operation->Buffer.Data) + 0 /*operation->Buffer.Offset*/);
            std::span<INPUT_RECORD> spr{ records, operation->Buffer.Size / sizeof(INPUT_RECORD) };
            for (auto&& i : spr)
            {
                if (i.EventType == KEY_EVENT)
                {
                    fmt::print(L"KEY {2} {0:02X} {1:04X}\n", i.Event.KeyEvent.wVirtualKeyCode, (wchar_t)(i.Event.KeyEvent.uChar.UnicodeChar), i.Event.KeyEvent.bKeyDown ? L"v" : L"^"); //L"\u2193" : L"\u2191");
                }
            }

            enqueueInput = TRUE;
        }
        WakeByAddressSingle(&enqueueInput);
        return S_FALSE;
    }
    HRESULT AllowUIAccess() const override
    {
        return S_FALSE;
    }
    ULONG_PTR PutHandle(const void* handle) override
    {
        // Whenever a handle is "registered" with us, we just pass it through as a literal int
        return reinterpret_cast<ULONG_PTR>(handle);
    }
    void* GetHandle(ULONG_PTR handle) const override
    {
        return reinterpret_cast<void*>(handle);
    }
    HRESULT GetServerHandle(HANDLE*) const override
    {
        // This is only called during handoff.
        return S_FALSE;
    }

    CD_CONNECTION_INFORMATION Connection{};
};

[[nodiscard]] HRESULT StartNullConsole(const ConsoleArguments* const args)
{
    auto& globals = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
    globals.pDeviceComm = new NullDeviceComm{}; // quickly, before we "connect". Leak this.

    // it is safe to pass INVALID_HANDLE_VALUE here because the null handle would have been detected
    // in ConDrvDeviceComm (which has been avoided by setting a global device comm beforehand)
    RETURN_IF_NTSTATUS_FAILED(ConsoleCreateIoThreadLegacy(INVALID_HANDLE_VALUE, args));

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT RunConhost()
{
    Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().hInstance = wil::GetModuleInstanceHandle();

    // passing stdin/stdout lets us drive this like conpty (!!) and test the VT renderer (!!)
    // but for now we want to drive it like conhost
    ConsoleArguments args({}, nullptr, nullptr);

    auto hr = args.ParseCommandline();
    if (SUCCEEDED(hr))
    {
        hr = StartNullConsole(&args);
    }

    return hr;
}

#ifdef FUZZING_BUILD
extern "C" __declspec(dllexport) int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
#else
int main(int /*argc*/, char** /*argv*/)
#endif
{
    RETURN_IF_FAILED(RunConhost());
    ExitThread(0);
    return 0;
}

extern "C" __declspec(dllexport) int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();

    const auto u16String{ til::u8u16(std::string_view{ reinterpret_cast<const char*>(data), size }) };
    til::CoordType scrollY{};
    gci.LockConsole();
    auto u = wil::scope_exit([&]() { gci.UnlockConsole(); });
    WriteCharsLegacy(gci.GetActiveOutputBuffer(), u16String, &scrollY);
    return 0;
}
