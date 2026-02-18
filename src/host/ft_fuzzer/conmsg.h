#pragma once

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
