#include "pch.h"
#include "Server.h"

#define CONSOLE_API_STRUCT(Routine, Struct) { &Routine, sizeof(Struct) }
#define CONSOLE_API_NO_PARAMETER(Routine) { &Routine, 0 }

struct ApiDescriptor
{
    NTSTATUS(Server::*routine)();
    size_t requiredSize;
};

struct ApiDescriptorLayer
{
    const ApiDescriptor* descriptors;
    size_t count;
};

static constexpr ApiDescriptor s_ptyServerUserL1[] = {
    CONSOLE_API_STRUCT(Server::handleUserL1GetConsoleCP, CONSOLE_GETCP_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1GetConsoleMode, CONSOLE_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1SetConsoleMode, CONSOLE_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1GetNumberOfConsoleInputEvents, CONSOLE_GETNUMBEROFINPUTEVENTS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1GetConsoleInput, CONSOLE_GETCONSOLEINPUT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1ReadConsole, CONSOLE_READCONSOLE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL1WriteConsole, CONSOLE_WRITECONSOLE_MSG),
    CONSOLE_API_NO_PARAMETER(Server::handleUserDeprecatedApi), // SrvConsoleNotifyLastClose
    CONSOLE_API_STRUCT(Server::handleUserL1GetConsoleLangId, CONSOLE_LANGID_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_MAPBITMAP_MSG),
};

static constexpr ApiDescriptor s_ptyServerUserL2[] = {
    CONSOLE_API_STRUCT(Server::handleUserL2FillConsoleOutput, CONSOLE_FILLCONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2GenerateConsoleCtrlEvent, CONSOLE_CTRLEVENT_MSG),
    CONSOLE_API_NO_PARAMETER(Server::handleUserL2SetConsoleActiveScreenBuffer),
    CONSOLE_API_NO_PARAMETER(Server::handleUserL2FlushConsoleInputBuffer),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleCP, CONSOLE_SETCP_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2GetConsoleCursorInfo, CONSOLE_GETCURSORINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleCursorInfo, CONSOLE_SETCURSORINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2GetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleScreenBufferInfo, CONSOLE_SCREENBUFFERINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleScreenBufferSize, CONSOLE_SETSCREENBUFFERSIZE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleCursorPosition, CONSOLE_SETCURSORPOSITION_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2GetLargestConsoleWindowSize, CONSOLE_GETLARGESTWINDOWSIZE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2ScrollConsoleScreenBuffer, CONSOLE_SCROLLSCREENBUFFER_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleTextAttribute, CONSOLE_SETTEXTATTRIBUTE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleWindowInfo, CONSOLE_SETWINDOWINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2ReadConsoleOutputString, CONSOLE_READCONSOLEOUTPUTSTRING_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2WriteConsoleInput, CONSOLE_WRITECONSOLEINPUT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2WriteConsoleOutput, CONSOLE_WRITECONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2WriteConsoleOutputString, CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2ReadConsoleOutput, CONSOLE_READCONSOLEOUTPUT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2GetConsoleTitle, CONSOLE_GETTITLE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL2SetConsoleTitle, CONSOLE_SETTITLE_MSG),
};

static constexpr ApiDescriptor s_ptyServerUserL3[] = {
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_GETNUMBEROFFONTS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleMouseInfo, CONSOLE_GETMOUSEINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_GETFONTINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleFontSize, CONSOLE_GETFONTSIZE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETFONT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETICON_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_INVALIDATERECT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_VDM_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETCURSOR_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SHOWCURSOR_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_MENUCONTROL_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETPALETTE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3SetConsoleDisplayMode, CONSOLE_SETDISPLAYMODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_REGISTERVDM_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_GETHARDWARESTATE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETHARDWARESTATE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleDisplayMode, CONSOLE_GETDISPLAYMODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3AddConsoleAlias, CONSOLE_ADDALIAS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleAlias, CONSOLE_GETALIAS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleAliasesLength, CONSOLE_GETALIASESLENGTH_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleAliasExesLength, CONSOLE_GETALIASEXESLENGTH_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleAliases, CONSOLE_GETALIASES_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleAliasExes, CONSOLE_GETALIASEXES_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3ExpungeConsoleCommandHistory, CONSOLE_EXPUNGECOMMANDHISTORY_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3SetConsoleNumberOfCommands, CONSOLE_SETNUMBEROFCOMMANDS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleCommandHistoryLength, CONSOLE_GETCOMMANDHISTORYLENGTH_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleCommandHistory, CONSOLE_GETCOMMANDHISTORY_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETKEYSHORTCUTS_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETMENUCLOSE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_GETKEYBOARDLAYOUTNAME_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleWindow, CONSOLE_GETCONSOLEWINDOW_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_CHAR_TYPE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_LOCAL_EUDC_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_CURSOR_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_REGISTEROS2_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_SETOS2OEMFORMAT_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserDeprecatedApi, CONSOLE_NLS_MODE_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleSelectionInfo, CONSOLE_GETSELECTIONINFO_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleProcessList, CONSOLE_GETCONSOLEPROCESSLIST_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3GetConsoleHistory, CONSOLE_HISTORY_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3SetConsoleHistory, CONSOLE_HISTORY_MSG),
    CONSOLE_API_STRUCT(Server::handleUserL3SetConsoleCurrentFont, CONSOLE_CURRENTFONT_MSG),
};

static constexpr ApiDescriptorLayer s_ptyServerUserLayers[] = {
    { s_ptyServerUserL1, std::size(s_ptyServerUserL1) },
    { s_ptyServerUserL2, std::size(s_ptyServerUserL2) },
    { s_ptyServerUserL3, std::size(s_ptyServerUserL3) },
};

NTSTATUS Server::handleUserDefined()
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

    // Pre-configure the response span to point at m_req.u (zero-copy).
    // Handlers that write results into m_req.u have them sent back automatically.
    // Mirrors the OG ConsoleDispatchRequest setting Complete.Write before calling the API.
    m_resData = { reinterpret_cast<const uint8_t*>(&m_req.u), m_req.msgHeader.ApiDescriptorSize };

    return (this->*descriptor.routine)();
}

NTSTATUS Server::handleUserDeprecatedApi()
{
    return STATUS_UNSUCCESSFUL;
}
