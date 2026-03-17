#include "pch.h"
#include "PtyServer.h"

#define CONSOLE_API_STRUCT(Routine, Struct) { &Routine, sizeof(Struct) }
#define CONSOLE_API_NO_PARAMETER(Routine) { &Routine, 0 }

struct ApiDescriptor
{
    NTSTATUS(PtyServer::*routine)();
    size_t requiredSize;
};

struct ApiDescriptorLayer
{
    const ApiDescriptor* descriptors;
    size_t count;
};

static constexpr ApiDescriptor s_ptyServerUserL1[] = {
    CONSOLE_API_STRUCT(PtyServer::handleUserL1GetConsoleCP, CONSOLE_GETCP_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1GetConsoleMode, CONSOLE_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1SetConsoleMode, CONSOLE_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1GetNumberOfConsoleInputEvents, CONSOLE_GETNUMBEROFINPUTEVENTS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1GetConsoleInput, CONSOLE_GETCONSOLEINPUT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1ReadConsole, CONSOLE_READCONSOLE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL1WriteConsole, CONSOLE_WRITECONSOLE_MSG),
    CONSOLE_API_NO_PARAMETER(PtyServer::handleUserDeprecatedApi), // SrvConsoleNotifyLastClose
    CONSOLE_API_STRUCT(PtyServer::handleUserL1GetConsoleLangId, CONSOLE_LANGID_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_MAPBITMAP_MSG),
};

static constexpr ApiDescriptor s_ptyServerUserL2[] = {
    CONSOLE_API_STRUCT(PtyServer::handleUserL2FillConsoleOutput, CONSOLE_FILLCONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2GenerateConsoleCtrlEvent, CONSOLE_CTRLEVENT_MSG),
    CONSOLE_API_NO_PARAMETER(PtyServer::handleUserL2SetConsoleActiveScreenBuffer),
    CONSOLE_API_NO_PARAMETER(PtyServer::handleUserL2FlushConsoleInputBuffer),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleCP, CONSOLE_SETCP_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2GetConsoleCursorInfo, CONSOLE_GETCURSORINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleCursorInfo, CONSOLE_SETCURSORINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2GetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleScreenBufferSize, CONSOLE_SETSCREENBUFFERSIZE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleCursorPosition, CONSOLE_SETCURSORPOSITION_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2GetLargestConsoleWindowSize, CONSOLE_GETLARGESTWINDOWSIZE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2ScrollConsoleScreenBuffer, CONSOLE_SCROLLSCREENBUFFER_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleTextAttribute, CONSOLE_SETTEXTATTRIBUTE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleWindowInfo, CONSOLE_SETWINDOWINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2ReadConsoleOutputString, CONSOLE_READCONSOLEOUTPUTSTRING_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2WriteConsoleInput, CONSOLE_WRITECONSOLEINPUT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2WriteConsoleOutput, CONSOLE_WRITECONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2WriteConsoleOutputString, CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2ReadConsoleOutput, CONSOLE_READCONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2GetConsoleTitle, CONSOLE_GETTITLE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL2SetConsoleTitle, CONSOLE_SETTITLE_MSG),
};

static constexpr ApiDescriptor s_ptyServerUserL3[] = {
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_GETNUMBEROFFONTS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleMouseInfo, CONSOLE_GETMOUSEINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_GETFONTINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleFontSize, CONSOLE_GETFONTSIZE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETFONT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETICON_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_INVALIDATERECT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_VDM_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETCURSOR_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SHOWCURSOR_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_MENUCONTROL_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETPALETTE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3SetConsoleDisplayMode, CONSOLE_SETDISPLAYMODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_REGISTERVDM_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_GETHARDWARESTATE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETHARDWARESTATE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleDisplayMode, CONSOLE_GETDISPLAYMODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3AddConsoleAlias, CONSOLE_ADDALIAS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleAlias, CONSOLE_GETALIAS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleAliasesLength, CONSOLE_GETALIASESLENGTH_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleAliasExesLength, CONSOLE_GETALIASEXESLENGTH_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleAliases, CONSOLE_GETALIASES_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleAliasExes, CONSOLE_GETALIASEXES_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3ExpungeConsoleCommandHistory, CONSOLE_EXPUNGECOMMANDHISTORY_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3SetConsoleNumberOfCommands, CONSOLE_SETNUMBEROFCOMMANDS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleCommandHistoryLength, CONSOLE_GETCOMMANDHISTORYLENGTH_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleCommandHistory, CONSOLE_GETCOMMANDHISTORY_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETKEYSHORTCUTS_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETMENUCLOSE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_GETKEYBOARDLAYOUTNAME_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleWindow, CONSOLE_GETCONSOLEWINDOW_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_CHAR_TYPE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_LOCAL_EUDC_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_REGISTEROS2_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_SETOS2OEMFORMAT_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserDeprecatedApi, CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleSelectionInfo, CONSOLE_GETSELECTIONINFO_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleProcessList, CONSOLE_GETCONSOLEPROCESSLIST_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3GetConsoleHistory, CONSOLE_HISTORY_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3SetConsoleHistory, CONSOLE_HISTORY_MSG),
    CONSOLE_API_STRUCT(PtyServer::handleUserL3SetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG),
};

static constexpr ApiDescriptorLayer s_ptyServerUserLayers[] = {
    { s_ptyServerUserL1, std::size(s_ptyServerUserL1) },
    { s_ptyServerUserL2, std::size(s_ptyServerUserL2) },
    { s_ptyServerUserL3, std::size(s_ptyServerUserL3) },
};

NTSTATUS PtyServer::handleUserDefined()
{
    const auto layer = (m_req.msgHeader.ApiNumber >> 24) - 1;
    const auto idx = m_req.msgHeader.ApiNumber & 0xffffff;

    if (layer >= std::size(s_ptyServerUserLayers) || idx >= s_ptyServerUserLayers[layer].count)
    {
        THROW_NTSTATUS(STATUS_ILLEGAL_FUNCTION);
    }

    const auto& descriptor = s_ptyServerUserLayers[layer].descriptors[idx];

    if (m_req.Descriptor.InputSize < sizeof(CONSOLE_MSG_HEADER) ||
        m_req.msgHeader.ApiDescriptorSize > sizeof(m_req.u) ||
        m_req.msgHeader.ApiDescriptorSize > (m_req.Descriptor.InputSize - sizeof(CONSOLE_MSG_HEADER)) ||
        m_req.msgHeader.ApiDescriptorSize < descriptor.requiredSize)
    {
        THROW_NTSTATUS(STATUS_ILLEGAL_FUNCTION);
    }

    // Pre-configure the Write blob for the piggyback response.
    // Handlers that write results into m_req.u have them sent back automatically.
    // Mirrors the OG ConsoleDispatchRequest setting Complete.Write before calling the API.
    m_resWriteData = &m_req.u;
    m_resWriteSize = m_req.msgHeader.ApiDescriptorSize;

    return (this->*descriptor.routine)();
}

NTSTATUS PtyServer::handleUserDeprecatedApi()
{
    return STATUS_UNSUCCESSFUL;
}
