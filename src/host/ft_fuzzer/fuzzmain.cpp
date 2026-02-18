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

template<int I>
struct ConsoleMessageTypeOracle
{
    using type = void;
};

#define CONSOLE_API_L(Number, InputBuffers, OutputBuffers)   \
    template<>                                               \
    struct ConsoleMessageTypeOracle<Number>                  \
    {                                                        \
        using has_body = std::false_type;                    \
        static constexpr int input_buffers = InputBuffers;   \
        static constexpr int output_buffers = OutputBuffers; \
    };

#define CONSOLE_API_Z(Number) CONSOLE_API_L(Number, 0, 0)

#define CONSOLE_API_B(Number, Struct, Member, InputBuffers, OutputBuffers)            \
    template<>                                                                        \
    struct ConsoleMessageTypeOracle<Number>                                           \
    {                                                                                 \
        using type = Struct;                                                          \
        using has_body = std::true_type;                                              \
        static constexpr int input_buffers = InputBuffers;                            \
        static constexpr int output_buffers = OutputBuffers;                          \
        static auto& memb(CONSOLE_API_MSG& message) { return message.u.Member = {}; } \
    };

#define CONSOLE_API_T(Number, Struct, Member) \
    CONSOLE_API_B(Number, Struct, Member, 0, 0)

#define CONSOLE_API_I(Number, Struct, Member) \
    CONSOLE_API_B(Number, Struct, Member, 1, 0)

#define CONSOLE_API_O(Number, Struct, Member) \
    CONSOLE_API_B(Number, Struct, Member, 0, 1)

CONSOLE_API_T(ConsolepGetCP, CONSOLE_GETCP_MSG, consoleMsgL1.GetConsoleCP)
CONSOLE_API_T(ConsolepGetMode, CONSOLE_MODE_MSG, consoleMsgL1.GetConsoleMode)
CONSOLE_API_T(ConsolepSetMode, CONSOLE_MODE_MSG, consoleMsgL1.SetConsoleMode)
CONSOLE_API_T(ConsolepGetNumberOfInputEvents, CONSOLE_GETNUMBEROFINPUTEVENTS_MSG, consoleMsgL1.GetNumberOfConsoleInputEvents)
CONSOLE_API_O(ConsolepGetConsoleInput, CONSOLE_GETCONSOLEINPUT_MSG, consoleMsgL1.GetConsoleInput)
CONSOLE_API_O(ConsolepReadConsole, CONSOLE_READCONSOLE_MSG, consoleMsgL1.ReadConsole)
CONSOLE_API_I(ConsolepWriteConsole, CONSOLE_WRITECONSOLE_MSG, consoleMsgL1.WriteConsole)
CONSOLE_API_T(ConsolepGetLangId, CONSOLE_LANGID_MSG, consoleMsgL1.GetConsoleLangId)
CONSOLE_API_T(ConsolepGenerateCtrlEvent, CONSOLE_CTRLEVENT_MSG, consoleMsgL2.GenerateConsoleCtrlEvent)
CONSOLE_API_T(ConsolepFillConsoleOutput, CONSOLE_FILLCONSOLEOUTPUT_MSG, consoleMsgL2.FillConsoleOutput)
CONSOLE_API_Z(ConsolepSetActiveScreenBuffer)
CONSOLE_API_Z(ConsolepFlushInputBuffer)
CONSOLE_API_T(ConsolepSetCP, CONSOLE_SETCP_MSG, consoleMsgL2.SetConsoleCP)
CONSOLE_API_T(ConsolepGetCursorInfo, CONSOLE_GETCURSORINFO_MSG, consoleMsgL2.GetConsoleCursorInfo)
CONSOLE_API_T(ConsolepSetCursorInfo, CONSOLE_SETCURSORINFO_MSG, consoleMsgL2.SetConsoleCursorInfo)
CONSOLE_API_T(ConsolepGetScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG, consoleMsgL2.GetConsoleScreenBufferInfo)
CONSOLE_API_T(ConsolepSetScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG, consoleMsgL2.SetConsoleScreenBufferInfo)
CONSOLE_API_T(ConsolepSetScreenBufferSize, CONSOLE_SETSCREENBUFFERSIZE_MSG, consoleMsgL2.SetConsoleScreenBufferSize)
CONSOLE_API_T(ConsolepSetCursorPosition, CONSOLE_SETCURSORPOSITION_MSG, consoleMsgL2.SetConsoleCursorPosition)
CONSOLE_API_T(ConsolepGetLargestWindowSize, CONSOLE_GETLARGESTWINDOWSIZE_MSG, consoleMsgL2.GetLargestConsoleWindowSize)
CONSOLE_API_T(ConsolepScrollScreenBuffer, CONSOLE_SCROLLSCREENBUFFER_MSG, consoleMsgL2.ScrollConsoleScreenBuffer)
CONSOLE_API_T(ConsolepSetTextAttribute, CONSOLE_SETTEXTATTRIBUTE_MSG, consoleMsgL2.SetConsoleTextAttribute)
CONSOLE_API_T(ConsolepSetWindowInfo, CONSOLE_SETWINDOWINFO_MSG, consoleMsgL2.SetConsoleWindowInfo)
CONSOLE_API_O(ConsolepReadConsoleOutputString, CONSOLE_READCONSOLEOUTPUTSTRING_MSG, consoleMsgL2.ReadConsoleOutputString)
CONSOLE_API_I(ConsolepWriteConsoleInput, CONSOLE_WRITECONSOLEINPUT_MSG, consoleMsgL2.WriteConsoleInput)
CONSOLE_API_I(ConsolepWriteConsoleOutput, CONSOLE_WRITECONSOLEOUTPUT_MSG, consoleMsgL2.WriteConsoleOutput)
CONSOLE_API_I(ConsolepWriteConsoleOutputString, CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG, consoleMsgL2.WriteConsoleOutputString)
CONSOLE_API_O(ConsolepReadConsoleOutput, CONSOLE_READCONSOLEOUTPUT_MSG, consoleMsgL2.ReadConsoleOutput)
CONSOLE_API_O(ConsolepGetTitle, CONSOLE_GETTITLE_MSG, consoleMsgL2.GetConsoleTitle)
CONSOLE_API_I(ConsolepSetTitle, CONSOLE_SETTITLE_MSG, consoleMsgL2.SetConsoleTitle)
CONSOLE_API_T(ConsolepGetMouseInfo, CONSOLE_GETMOUSEINFO_MSG, consoleMsgL3.GetConsoleMouseInfo)
CONSOLE_API_T(ConsolepGetFontSize, CONSOLE_GETFONTSIZE_MSG, consoleMsgL3.GetConsoleFontSize)
CONSOLE_API_L(ConsolepGetCurrentFont, 0, 1)
CONSOLE_API_T(ConsolepSetDisplayMode, CONSOLE_SETDISPLAYMODE_MSG, consoleMsgL3.SetConsoleDisplayMode)
CONSOLE_API_T(ConsolepGetDisplayMode, CONSOLE_GETDISPLAYMODE_MSG, consoleMsgL3.GetConsoleDisplayMode)
CONSOLE_API_I(ConsolepAddAlias, CONSOLE_ADDALIAS_MSG, consoleMsgL3.AddConsoleAlias)
CONSOLE_API_B(ConsolepGetAlias, CONSOLE_GETALIAS_MSG, consoleMsgL3.GetConsoleAlias, 1, 1)
CONSOLE_API_I(ConsolepGetAliasesLength, CONSOLE_GETALIASESLENGTH_MSG, consoleMsgL3.GetConsoleAliasesLength)
CONSOLE_API_T(ConsolepGetAliasExesLength, CONSOLE_GETALIASEXESLENGTH_MSG, consoleMsgL3.GetConsoleAliasExesLength)
CONSOLE_API_B(ConsolepGetAliases, CONSOLE_GETALIASES_MSG, consoleMsgL3.GetConsoleAliases, 1, 1)
CONSOLE_API_O(ConsolepGetAliasExes, CONSOLE_GETALIASEXES_MSG, consoleMsgL3.GetConsoleAliasExes)
CONSOLE_API_I(ConsolepExpungeCommandHistory, CONSOLE_EXPUNGECOMMANDHISTORY_MSG, consoleMsgL3.ExpungeConsoleCommandHistory)
CONSOLE_API_I(ConsolepSetNumberOfCommands, CONSOLE_SETNUMBEROFCOMMANDS_MSG, consoleMsgL3.SetConsoleNumberOfCommands)
CONSOLE_API_I(ConsolepGetCommandHistoryLength, CONSOLE_GETCOMMANDHISTORYLENGTH_MSG, consoleMsgL3.GetConsoleCommandHistoryLength)
CONSOLE_API_B(ConsolepGetCommandHistory, CONSOLE_GETCOMMANDHISTORY_MSG, consoleMsgL3.GetConsoleCommandHistory, 1, 1)
CONSOLE_API_T(ConsolepGetConsoleWindow, CONSOLE_GETCONSOLEWINDOW_MSG, consoleMsgL3.GetConsoleWindow)
CONSOLE_API_T(ConsolepGetSelectionInfo, CONSOLE_GETSELECTIONINFO_MSG, consoleMsgL3.GetConsoleSelectionInfo)
CONSOLE_API_T(ConsolepGetConsoleProcessList, CONSOLE_GETCONSOLEPROCESSLIST_MSG, consoleMsgL3.GetConsoleProcessList)
CONSOLE_API_T(ConsolepGetHistory, CONSOLE_HISTORY_MSG, consoleMsgL3.GetConsoleHistory)
CONSOLE_API_T(ConsolepSetHistory, CONSOLE_HISTORY_MSG, consoleMsgL3.SetConsoleHistory)
CONSOLE_API_T(ConsolepSetCurrentFont, CONSOLE_CURRENTFONT_MSG, consoleMsgL3.SetCurrentConsoleFont)

template<int Id>
auto& PrepareConsoleMessage(CONSOLE_API_MSG& message)
{
    message.Descriptor.Function = CONSOLE_IO_USER_DEFINED;
    using orl = ConsoleMessageTypeOracle<Id>;
    message.msgHeader.ApiNumber = Id;
    if constexpr (orl::has_body::value)
    {
        using t = typename orl::type;
        message.Descriptor.InputSize = sizeof(t) + sizeof(CONSOLE_MSG_HEADER);
        message.msgHeader.ApiDescriptorSize = sizeof(t);
        return orl::memb(message);
    }
    else
    {
        message.Descriptor.InputSize = sizeof(CONSOLE_MSG_HEADER);
        message.msgHeader.ApiDescriptorSize = 0;
        return 0;
    }
}

template<int Id>
const auto& ReadConsoleMessage(CONSOLE_API_MSG& message)
{
    using orl = ConsoleMessageTypeOracle<Id>;
    if constexpr (orl::has_body::value)
    {
        return orl::memb(message);
    }
    else
    {
        return 0;
    }
}

struct NullDeviceComm : public IDeviceComm
{
    static constexpr DWORD EnqueuedReadInputMessage = 0xAA000000;
    mutable BOOL enqueueInput{ TRUE };

    void _RetireIoOperation(CD_IO_COMPLETE* const complete) const
    {
        if (complete->Identifier.LowPart == 0)
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
        static DWORD ident = 0;

        if (pReplyMessage)
        {
            fmt::print(L"OP-{} Reply During Read\n", pReplyMessage->Complete.Identifier.LowPart);
            _RetireIoOperation(&pReplyMessage->Complete);
        }

        CONSOLE_API_MSG local{};
        if (ident == 0)
        {
            local.Descriptor.Identifier.LowPart = ident++;
            local.Descriptor.Function = CONSOLE_IO_CONNECT;
            local.Descriptor.Process = GetCurrentProcessId(); // CAC - Process=PID
            local.Descriptor.Object = GetCurrentThreadId(); // CAC - Object=TID
            local.Descriptor.InputSize = sizeof(CONSOLE_SERVER_MSG);
        }
        else if (ident == 1)
        {
            local.Descriptor.Identifier.LowPart = ident++;
            local.Descriptor.Object = Connection.Output;
            local.Descriptor.Process = Connection.Process;
            auto& modeMsg = PrepareConsoleMessage<ConsolepSetMode>(local);
            modeMsg.Mode = 0x3 | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        }
        else if (ident == 2 || ident == 4)
        {
            local.Descriptor.Identifier.LowPart = ident++;
            local.Descriptor.Function = CONSOLE_IO_RAW_WRITE;
            local.Descriptor.Object = Connection.Output;
            local.Descriptor.Process = Connection.Process;
            local.Descriptor.InputSize = 6;
        }
        else if (ident == 3)
        {
            local.Descriptor.Identifier.LowPart = ident++;
            local.Descriptor.Object = Connection.Input;
            local.Descriptor.Process = Connection.Process;
            auto& modeMsg = PrepareConsoleMessage<ConsolepSetMode>(local);
            modeMsg.Mode = ENABLE_VIRTUAL_TERMINAL_INPUT;
        }
        else if (ident == 5)
        {
            local.Descriptor.Identifier.LowPart = ident++;
            local.Descriptor.Object = Connection.Output;
            local.Descriptor.Process = Connection.Process;
            auto& msg = PrepareConsoleMessage<ConsolepFillConsoleOutput>(local);
            msg = {
                .WriteCoord = { 0, 0 },
                .ElementType = CONSOLE_ATTRIBUTE,
                .Element = 0x43,
                .Length = 40,
            };
        }
        else if (ident == 6)
        {
            BOOL old{FALSE};
            while (!enqueueInput)
                WaitOnAddress(&enqueueInput, &old, sizeof(enqueueInput), INFINITE);
            if (enqueueInput)
            {
                local.Descriptor.Identifier.LowPart = EnqueuedReadInputMessage;
                local.Descriptor.Function = CONSOLE_IO_RAW_READ;
                local.Descriptor.Object = Connection.Input;
                local.Descriptor.Process = Connection.Process;
                local.Descriptor.InputSize = 0;
                local.Descriptor.OutputSize = 1024;
                enqueueInput = FALSE;
            }
        }
        memcpy(&pMessage->Descriptor, &local.Descriptor, sizeof(CONSOLE_API_MSG) - FIELD_OFFSET(CONSOLE_API_MSG, Descriptor));
        // The easiest way to get the IO thread to stop reading from us us to simply
        // suspend it. The fuzzer doesn't need a device IO thread.
        return S_FALSE;
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
        switch (operation->Identifier.LowPart)
        {
        case 0:
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
        case 2:
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
        enqueueInput = TRUE;
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
