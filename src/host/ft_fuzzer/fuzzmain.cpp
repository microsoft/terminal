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
#include <til/spsc.h>
#include <til/mutex.h>

struct EnqueuedMessage
{
    std::unique_ptr<uint8_t[]> outputBuffer;
    std::unique_ptr<uint8_t[]> inputBuffer;
    CD_IO_DESCRIPTOR Descriptor;
    union
    {
        struct
        {
            CD_CREATE_OBJECT_INFORMATION CreateObject;
            CONSOLE_CREATESCREENBUFFER_MSG CreateScreenBuffer;
        };
        struct
        {
            CONSOLE_MSG_HEADER msgHeader;
            union
            {
                CONSOLE_MSG_BODY_L1 consoleMsgL1;
                CONSOLE_MSG_BODY_L2 consoleMsgL2;
                CONSOLE_MSG_BODY_L3 consoleMsgL3;
            } u;
        };
    };
};

struct Rd
{
    std::unique_ptr<uint8_t[]> buf;
    size_t len;
    uint64_t seq; // assigned at queue time
};

struct Owc
{
    std::list<Rd> q;
};

static void
FillLuid(LUID& l, uint64_t seq)
{
    l.HighPart = static_cast<LONG>((seq & 0xFFFFFFFF00000000ULL) >> 32ULL);
    l.LowPart = static_cast<DWORD>(seq & DWORD_MAX);
}

static uint64_t GetLuid(const LUID& l)
{
    return static_cast<uint64_t>(l.HighPart) << 32ULL | static_cast<uint64_t>(l.LowPart);
}

static void hdl(std::span<INPUT_RECORD> buffer)
{
    std::optional<wchar_t> _highSurrogate;
    std::wstring _convertedString;
    for (auto it = buffer.begin(); it != buffer.end(); ++it)
    {
        if (it->EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            // TIOCSWINSZ
            //_windowSizeChangedCallback();
        }
        else if (it->EventType == KEY_EVENT)
        {
            const auto& keyEvent = it->Event.KeyEvent;
            if (keyEvent.bKeyDown || (!keyEvent.bKeyDown && keyEvent.wVirtualKeyCode == VK_MENU))
            {
                // Got a high surrogate at the end of the buffer
                if (IS_HIGH_SURROGATE(keyEvent.uChar.UnicodeChar))
                {
                    _highSurrogate.emplace(keyEvent.uChar.UnicodeChar);
                    continue; // we've consumed it -- only dispatch it if we get a low
                }

                if (IS_LOW_SURROGATE(keyEvent.uChar.UnicodeChar))
                {
                    // No matter what we do, we want to destructively consume the high surrogate
                    if (const auto oldHighSurrogate{ std::exchange(_highSurrogate, std::nullopt) })
                    {
                        _convertedString.push_back(*oldHighSurrogate);
                    }
                    else
                    {
                        // If we get a low without a high surrogate, we've done everything we can.
                        // This is an illegal state.
                        _convertedString.push_back(UNICODE_REPLACEMENT);
                        continue; // onto the next event
                    }
                }

                // (\0 with a scancode is probably a modifier key, not a VT input key)
                if (keyEvent.uChar.UnicodeChar != L'\0' || keyEvent.wVirtualScanCode == 0)
                {
                    if (_highSurrogate) // non-destructive: we don't want to set it to nullopt needlessly for every character
                    {
                        // If we get a high surrogate *here*, we didn't find a low surrogate.
                        // This state is also illegal.
                        _convertedString.push_back(UNICODE_REPLACEMENT);
                        _highSurrogate.reset();
                    }
                    _convertedString.push_back(keyEvent.uChar.UnicodeChar);
                }
            }
        }
    }
    auto eightString = til::u16u8(_convertedString);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), eightString.data(), (DWORD)eightString.size(), nullptr, nullptr);
}

struct NullDeviceComm : public IDeviceComm
{
    mutable uint64_t _seq{ 1 };
    static constexpr DWORD InitialConnectMessage = 0xA0000000;
    static constexpr DWORD EnqueuedReadInputMessage = 0xAA000000;
    mutable std::atomic<BOOL> signal{ TRUE }; // start out true to kick off an enqueue
    mutable BOOL enqueueInput{ TRUE };
    mutable Owc _outgoingWriteConsole;

    void _RetireIoOperation(CD_IO_COMPLETE* const complete) const
    {
        if (complete->Identifier.LowPart == InitialConnectMessage)
        {
            memcpy((void*)&Connection, reinterpret_cast<uint8_t*>(complete->Write.Data) + complete->Write.Offset, std::min(sizeof(Connection), (unsigned long long)complete->Write.Size));
        }
        else
        {
            auto head = _outgoingWriteConsole.q.begin();
            if (head != _outgoingWriteConsole.q.end() && head->seq == GetLuid(complete->Identifier))
            {
                _outgoingWriteConsole.q.pop_front();
                signal = signal || _outgoingWriteConsole.q.size() > 0;
            }
        }
        fmt::print(stderr, L"OP-{} Retired\n", complete->Identifier.LowPart);
    }

    HRESULT SetServerInformation(CD_IO_SERVER_INFORMATION* const) const override
    {
        return S_FALSE;
    }
    HRESULT ReadIo(PCONSOLE_API_MSG pReplyMessage, CONSOLE_API_MSG* const pMessage) const override
    {
        if (pReplyMessage)
        {
            fmt::print(stderr, L"OP-{} Reply Received During Read\n", pReplyMessage->Complete.Identifier.LowPart);
            _RetireIoOperation(&pReplyMessage->Complete);
        }

        CONSOLE_API_MSG& local = *pMessage;
        memset(&local.Descriptor, 0, sizeof(CONSOLE_API_MSG) - FIELD_OFFSET(CONSOLE_API_MSG, Descriptor));

        if (!Connection.Process)
        {
            local.Descriptor.Identifier.LowPart = InitialConnectMessage;
            local.Descriptor.Function = CONSOLE_IO_CONNECT;
            local.Descriptor.Process = GetCurrentProcessId(); // CAC - Process=PID
            local.Descriptor.Object = GetCurrentThreadId(); // CAC - Object=TID
            local.Descriptor.InputSize = sizeof(CONSOLE_SERVER_MSG);
            // Populated pMessage; send it
            return S_OK;
        }

        local.Descriptor.Process = Connection.Process;
        auto seq = _seq++;

        FillLuid(local.Descriptor.Identifier, seq);

        switch (seq)
        {
        case 1:
        {
            local.Descriptor.Object = Connection.Output;
            auto& msg = PrepareConsoleMessage<ConsolepSetMode>(local);
            msg.Mode = 0x3 | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
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
            local.Descriptor.Object = Connection.Output;
            auto& msg = PrepareConsoleMessage<ConsolepSetCP>(local);
            msg.CodePage = CP_UTF8;
            msg.Output = seq == 3 ? TRUE : FALSE;
            break;
        }
#if 0
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
#endif
        default:
        {
            BOOL old{ FALSE };
            while (!signal.load())
                til::atomic_wait(signal, old); // wait for a signal

            if (enqueueInput)
            {
                local.Descriptor.Identifier.LowPart = EnqueuedReadInputMessage;
                auto& msg = PrepareConsoleMessage<ConsolepGetConsoleInput>(local);
                msg.Unicode = TRUE;
                local.Descriptor.Object = Connection.Input;
                local.Descriptor.OutputSize = 128 * 1024;
                enqueueInput = FALSE;
                signal = _outgoingWriteConsole.q.size() > 0;
                return S_OK;
            }

            if (_outgoingWriteConsole.q.size())
            {
                auto head = _outgoingWriteConsole.q.begin();
                head->seq = seq;
                local.Descriptor.Function = CONSOLE_IO_RAW_WRITE;
                local.Descriptor.Object = Connection.Output;
                local.Descriptor.InputSize = static_cast<ULONG>(head->len);
                signal = enqueueInput; // if we should also enqueue an input on the next trip around
                return S_OK;
            }

            return S_OK;
        }
        }

        //send:
        //memcpy(&pMessage->Descriptor, &local.Descriptor, sizeof(CONSOLE_API_MSG) - FIELD_OFFSET(CONSOLE_API_MSG, Descriptor));
        // The easiest way to get the IO thread to stop reading from us us to simply
        // suspend it. The fuzzer doesn't need a device IO thread.
        return S_OK;
    }
    HRESULT CompleteIo(CD_IO_COMPLETE* const completion) const override
    {
        fmt::print(stderr, L"OP-{} Completed\n", completion->Identifier.LowPart);
        _RetireIoOperation(completion);
        return S_OK;
    }
    HRESULT ReadInput(CD_IO_OPERATION* const operation) const override
    {
        fmt::print(stderr, L"OP-{} ReadInput\n", operation->Identifier.LowPart);
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

        auto head = _outgoingWriteConsole.q.begin();
        if (head->seq == GetLuid(operation->Identifier))
        {
            fmt::print(stderr, L"OP-{} Filling Get request for {} (dest 0x{:016X} + 0x{:X})\n", head->seq, operation->Buffer.Size, (uintptr_t)(operation->Buffer.Data), operation->Buffer.Offset);
            memcpy((uint8_t*)(operation->Buffer.Data) + operation->Buffer.Offset, head->buf.get(), std::min(static_cast<size_t>(operation->Buffer.Size), head->len));
            return S_OK;
        }

        fmt::print(stderr, L"OP-{} OUT OF ORDER READ???\n", head->seq, operation->Buffer.Size);
        return S_FALSE;
    }
    HRESULT WriteOutput(CD_IO_OPERATION* const operation) const override
    {
        fmt::print(stderr, L"OP-{} WriteOutput\n", operation->Identifier.LowPart);
        fmt::print(stderr, L"Writing output of size {}\n", operation->Buffer.Size);
        if (operation->Identifier.LowPart == EnqueuedReadInputMessage)
        {
            // TODO(DH) - figure out why Offset should be ignored here. It looks like that's how CompleteIo gets the _message_ back, but it's unused here?
            auto records = reinterpret_cast<INPUT_RECORD*>(reinterpret_cast<uint8_t*>(operation->Buffer.Data) + 0 /*operation->Buffer.Offset*/);
            std::span<INPUT_RECORD> spr{ records, operation->Buffer.Size / sizeof(INPUT_RECORD) };
            hdl(spr);
            #if 0
            for (auto&& i : spr)
            {
                if (i.EventType == KEY_EVENT)
                {
                    fmt::print(stderr, L"KEY {2} {0:02X} {1:04X}\n", i.Event.KeyEvent.wVirtualKeyCode, (wchar_t)(i.Event.KeyEvent.uChar.UnicodeChar), i.Event.KeyEvent.bKeyDown ? L"v" : L"^"); //L"\u2193" : L"\u2191");
                }
            }
            #endif

            enqueueInput = TRUE;
            signal.store(TRUE);
            til::atomic_notify_one(signal);
        }
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

    void WriteData(std::unique_ptr<uint8_t[]> buffer, size_t len)
    {
        _outgoingWriteConsole.q.emplace_back(Rd{ std::move(buffer), len, 0 });
        signal.store(TRUE);
        til::atomic_notify_one(signal);
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
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    RETURN_IF_FAILED(RunConhost());
    std::thread([]() {
        auto& globals = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        auto n = static_cast<NullDeviceComm*>(globals.pDeviceComm);
        while (true)
        {
            static constexpr int bsz = 4096;
            std::unique_ptr<uint8_t[]> buf = std::make_unique_for_overwrite<uint8_t[]>(bsz);
            DWORD read;
            ReadFile(GetStdHandle(STD_INPUT_HANDLE), buf.get(), bsz, &read, nullptr);
            n->WriteData(std::move(buf), read);
        }
    }).detach();
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
