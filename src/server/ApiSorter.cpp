// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ApiSorter.h"

#include "ApiDispatchers.h"

#include "../host/tracing.hpp"

#define CONSOLE_API_STRUCT(Routine, Struct, TraceName) \
    {                                                  \
        Routine, sizeof(Struct), TraceName             \
    }
#define CONSOLE_API_NO_PARAMETER(Routine, TraceName) \
    {                                                \
        Routine, 0, TraceName                        \
    }

#define CONSOLE_API_DEPRECATED(Struct)                                    \
    {                                                                     \
        ApiDispatchers::ServerDeprecatedApi, sizeof(Struct), "Deprecated" \
    }
#define CONSOLE_API_DEPRECATED_NO_PARAM()                    \
    {                                                        \
        ApiDispatchers::ServerDeprecatedApi, 0, "Deprecated" \
    }

typedef struct _CONSOLE_API_DESCRIPTOR
{
    PCONSOLE_API_ROUTINE Routine;
    ULONG RequiredSize;
    PCSTR TraceName;
} CONSOLE_API_DESCRIPTOR, *PCONSOLE_API_DESCRIPTOR;

typedef struct _CONSOLE_API_LAYER_DESCRIPTOR
{
    const CONSOLE_API_DESCRIPTOR* Descriptor;
    ULONG Count;
} CONSOLE_API_LAYER_DESCRIPTOR, *PCONSOLE_API_LAYER_DESCRIPTOR;

const CONSOLE_API_DESCRIPTOR ConsoleApiLayer1[] = {
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleCP, CONSOLE_GETCP_MSG, "GetConsoleCP"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleMode, CONSOLE_MODE_MSG, "GetConsoleMode"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleMode, CONSOLE_MODE_MSG, "SetConsoleMode"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetNumberOfInputEvents, CONSOLE_GETNUMBEROFINPUTEVENTS_MSG, "GetNumberOfConsoleInputEvents"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleInput, CONSOLE_GETCONSOLEINPUT_MSG, "GetConsoleInput"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerReadConsole, CONSOLE_READCONSOLE_MSG, "ReadConsole"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerWriteConsole, CONSOLE_WRITECONSOLE_MSG, "WriteConsole"),
    CONSOLE_API_DEPRECATED_NO_PARAM(), // ApiDispatchers::ServerConsoleNotifyLastClose
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleLangId, CONSOLE_LANGID_MSG, "GetConsoleLangId"),
    CONSOLE_API_DEPRECATED(CONSOLE_MAPBITMAP_MSG),
};

const CONSOLE_API_DESCRIPTOR ConsoleApiLayer2[] = {
    CONSOLE_API_STRUCT(ApiDispatchers::ServerFillConsoleOutput, CONSOLE_FILLCONSOLEOUTPUT_MSG, "FillConsoleOutput"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGenerateConsoleCtrlEvent, CONSOLE_CTRLEVENT_MSG, "GenerateConsoleCtrlEvent"),
    CONSOLE_API_NO_PARAMETER(ApiDispatchers::ServerSetConsoleActiveScreenBuffer, "SetConsoleActiveScreenBuffer"),
    CONSOLE_API_NO_PARAMETER(ApiDispatchers::ServerFlushConsoleInputBuffer, "FlushConsoleInputBuffer"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleCP, CONSOLE_SETCP_MSG, "SetConsoleCP"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleCursorInfo, CONSOLE_GETCURSORINFO_MSG, "GetConsoleCursorInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleCursorInfo, CONSOLE_SETCURSORINFO_MSG, "SetConsoleCursorInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG, "GetConsoleScreenBufferInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG, "SetConsoleScreenBufferInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleScreenBufferSize, CONSOLE_SETSCREENBUFFERSIZE_MSG, "SetConsoleScreenBufferSize"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleCursorPosition, CONSOLE_SETCURSORPOSITION_MSG, "SetConsoleCursorPosition"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetLargestConsoleWindowSize, CONSOLE_GETLARGESTWINDOWSIZE_MSG, "GetLargestConsoleWindowSize"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerScrollConsoleScreenBuffer, CONSOLE_SCROLLSCREENBUFFER_MSG, "ScrollConsoleScreenBuffer"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleTextAttribute, CONSOLE_SETTEXTATTRIBUTE_MSG, "SetConsoleTextAttribute"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleWindowInfo, CONSOLE_SETWINDOWINFO_MSG, "SetConsoleWindowInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerReadConsoleOutputString, CONSOLE_READCONSOLEOUTPUTSTRING_MSG, "ReadConsoleOutputString"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerWriteConsoleInput, CONSOLE_WRITECONSOLEINPUT_MSG, "WriteConsoleInput"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerWriteConsoleOutput, CONSOLE_WRITECONSOLEOUTPUT_MSG, "WriteConsoleOutput"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerWriteConsoleOutputString, CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG, "WriteConsoleOutputString"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerReadConsoleOutput, CONSOLE_READCONSOLEOUTPUT_MSG, "ReadConsoleOutput"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleTitle, CONSOLE_GETTITLE_MSG, "GetConsoleTitle"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleTitle, CONSOLE_SETTITLE_MSG, "SetConsoleTitle"),
};

const CONSOLE_API_DESCRIPTOR ConsoleApiLayer3[] = {
    CONSOLE_API_DEPRECATED(CONSOLE_GETNUMBEROFFONTS_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleMouseInfo, CONSOLE_GETMOUSEINFO_MSG, "GetNumberOfConsoleMouseButtons"),
    CONSOLE_API_DEPRECATED(CONSOLE_GETFONTINFO_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleFontSize, CONSOLE_GETFONTSIZE_MSG, "GetConsoleFontSize"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG, "GetCurrentConsoleFont"),
    CONSOLE_API_DEPRECATED(CONSOLE_SETFONT_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETICON_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_INVALIDATERECT_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_VDM_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETCURSOR_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SHOWCURSOR_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_MENUCONTROL_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETPALETTE_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleDisplayMode, CONSOLE_SETDISPLAYMODE_MSG, "SetConsoleDisplayMode"),
    CONSOLE_API_DEPRECATED(CONSOLE_REGISTERVDM_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_GETHARDWARESTATE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETHARDWARESTATE_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleDisplayMode, CONSOLE_GETDISPLAYMODE_MSG, "GetConsoleDisplayMode"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerAddConsoleAlias, CONSOLE_ADDALIAS_MSG, "AddConsoleAlias"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleAlias, CONSOLE_GETALIAS_MSG, "GetConsoleAlias"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleAliasesLength, CONSOLE_GETALIASESLENGTH_MSG, "GetConsoleAliasesLength"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleAliasExesLength, CONSOLE_GETALIASEXESLENGTH_MSG, "GetConsoleAliasExesLength"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleAliases, CONSOLE_GETALIASES_MSG, "GetConsoleAliases"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleAliasExes, CONSOLE_GETALIASEXES_MSG, "GetConsoleAliasExes"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerExpungeConsoleCommandHistory, CONSOLE_EXPUNGECOMMANDHISTORY_MSG, "ExpungeConsoleCommandHistory"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleNumberOfCommands, CONSOLE_SETNUMBEROFCOMMANDS_MSG, "SetConsoleNumberOfCommands"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleCommandHistoryLength, CONSOLE_GETCOMMANDHISTORYLENGTH_MSG, "GetConsoleCommandHistoryLength"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleCommandHistory, CONSOLE_GETCOMMANDHISTORY_MSG, "GetConsoleCommandHistory"),
    CONSOLE_API_DEPRECATED(CONSOLE_SETKEYSHORTCUTS_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETMENUCLOSE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_GETKEYBOARDLAYOUTNAME_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleWindow, CONSOLE_GETCONSOLEWINDOW_MSG, "GetConsoleWindow"),
    CONSOLE_API_DEPRECATED(CONSOLE_CHAR_TYPE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_LOCAL_EUDC_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_REGISTEROS2_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_SETOS2OEMFORMAT_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_DEPRECATED(CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleSelectionInfo, CONSOLE_GETSELECTIONINFO_MSG, "GetConsoleSelectionInfo"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleProcessList, CONSOLE_GETCONSOLEPROCESSLIST_MSG, "GetConsoleProcessList"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerGetConsoleHistory, CONSOLE_HISTORY_MSG, "GetConsoleHistory"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleHistory, CONSOLE_HISTORY_MSG, "SetConsoleHistory"),
    CONSOLE_API_STRUCT(ApiDispatchers::ServerSetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG, "SetConsoleCurrentFont")
};

const CONSOLE_API_LAYER_DESCRIPTOR ConsoleApiLayerTable[] = {
    { ConsoleApiLayer1, RTL_NUMBER_OF(ConsoleApiLayer1) },
    { ConsoleApiLayer2, RTL_NUMBER_OF(ConsoleApiLayer2) },
    { ConsoleApiLayer3, RTL_NUMBER_OF(ConsoleApiLayer3) },
};

// Routine Description:
// - This routine validates a user IO and dispatches it to the appropriate worker routine.
// Arguments:
// - Message - Supplies the message representing the user IO.
// Return Value:
// - A pointer to the reply message, if this message is to be completed inline; nullptr if this message will pend now and complete later.
PCONSOLE_API_MSG ApiSorter::ConsoleDispatchRequest(_Inout_ PCONSOLE_API_MSG Message)
{
    // Make sure the indices are valid and retrieve the API descriptor.
    ULONG const LayerNumber = (Message->msgHeader.ApiNumber >> 24) - 1;
    ULONG const ApiNumber = Message->msgHeader.ApiNumber & 0xffffff;

    NTSTATUS Status;
    if ((LayerNumber >= RTL_NUMBER_OF(ConsoleApiLayerTable)) || (ApiNumber >= ConsoleApiLayerTable[LayerNumber].Count))
    {
        Status = STATUS_ILLEGAL_FUNCTION;
        goto Complete;
    }

    CONSOLE_API_DESCRIPTOR const* Descriptor = &ConsoleApiLayerTable[LayerNumber].Descriptor[ApiNumber];

    // Validate the argument size and call the API.
    if ((Message->Descriptor.InputSize < sizeof(CONSOLE_MSG_HEADER)) ||
        (Message->msgHeader.ApiDescriptorSize > sizeof(Message->u)) ||
        (Message->msgHeader.ApiDescriptorSize > Message->Descriptor.InputSize - sizeof(CONSOLE_MSG_HEADER)) ||
        (Message->msgHeader.ApiDescriptorSize < Descriptor->RequiredSize))
    {
        Status = STATUS_ILLEGAL_FUNCTION;
        goto Complete;
    }

    BOOL ReplyPending = FALSE;
    Message->Complete.Write.Data = &Message->u;
    Message->Complete.Write.Size = Message->msgHeader.ApiDescriptorSize;
    Message->State.WriteOffset = Message->msgHeader.ApiDescriptorSize;
    Message->State.ReadOffset = Message->msgHeader.ApiDescriptorSize + sizeof(CONSOLE_MSG_HEADER);

    // Unfortunately, we can't be as clear-cut with our error codes as we'd like since we have some callers that take
    // hard dependencies on NTSTATUS codes that aren't readily expressible as an HRESULT. There's currently only one
    // such known code -- STATUS_BUFFER_TOO_SMALL. There's a conlibk dependency on this being returned from the console
    // alias API.
    {
        const auto trace = Tracing::s_TraceApiCall(Status, Descriptor->TraceName);
        Status = (*Descriptor->Routine)(Message, &ReplyPending);
    }
    if (Status != STATUS_BUFFER_TOO_SMALL)
    {
        Status = NTSTATUS_FROM_HRESULT(Status);
    }

    if (!ReplyPending)
    {
        goto Complete;
    }

    return nullptr;

Complete:

    Message->SetReplyStatus(Status);

    return Message;
}
