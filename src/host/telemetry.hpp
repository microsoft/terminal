/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- telemetry.hpp

Abstract:
- This module is used for recording all telemetry feedback from the console

Author(s):
- Evan Wirt       (EvanWi)    09-Jul-2014
- Kourosh Mehrain (KMehrain)  09-Jul-2014
- Stephen Somuah  (StSomuah)  09-Jul-2014
- Anup Manandhar  (AnupM)     09-Jul-2014
--*/
#pragma once

#include <TraceLoggingActivity.h>

class Telemetry
{
public:
    // Implement this as a singleton class.
    static Telemetry& Instance()
    {
        static Telemetry s_Instance;
        return s_Instance;
    }

    void SetUserInteractive();
    void SetWindowSizeChanged();
    void SetContextMenuUsed();
    void SetKeyboardTextSelectionUsed();
    void SetKeyboardTextEditingUsed();
    void SetCtrlPgUpPgDnUsed();
    void LogCtrlShiftCProcUsed();
    void LogCtrlShiftCRawUsed();
    void LogCtrlShiftVProcUsed();
    void LogCtrlShiftVRawUsed();
    void LogQuickEditCopyProcUsed();
    void LogQuickEditCopyRawUsed();
    void LogQuickEditPasteProcUsed();
    void LogQuickEditPasteRawUsed();
    void LogColorSelectionUsed();

    void LogFindDialogNextClicked(const unsigned int iStringLength, const bool fDirectionDown, const bool fMatchCase);
    void LogProcessConnected(const HANDLE hProcess);
    void FindDialogClosed();
    void WriteFinalTraceLog();

    void LogRipMessage(_In_z_ const char* pszMessage, ...) const;

    // Names are from the external API call names.  Note that some names can be different
    // than the internal API calls.
    // Don't worry about the following APIs, because they are external to our conhost codebase and hard to track through
    // telemetry: GetStdHandle, SetConsoleCtrlHandler, SetStdHandle
    // We can't differentiate between these apis, so just log the "-Ex" versions: GetConsoleScreenBufferInfo / GetConsoleScreenBufferInfoEx,
    // GetCurrentConsoleFontEx / GetCurrentConsoleFont
    enum ApiCall
    {
        AddConsoleAlias = 0,
        AllocConsole,
        AttachConsole,
        CreateConsoleScreenBuffer,
        FillConsoleOutputAttribute,
        FillConsoleOutputCharacter,
        FlushConsoleInputBuffer,
        FreeConsole,
        GenerateConsoleCtrlEvent,
        GetConsoleAlias,
        GetConsoleAliases,
        GetConsoleAliasesLength,
        GetConsoleAliasExes,
        GetConsoleAliasExesLength,
        GetConsoleCP,
        GetConsoleCursorInfo,
        GetConsoleDisplayMode,
        GetConsoleFontSize,
        GetConsoleHistoryInfo,
        GetConsoleMode,
        GetConsoleLangId,
        GetConsoleOriginalTitle,
        GetConsoleOutputCP,
        GetConsoleProcessList,
        GetConsoleScreenBufferInfoEx,
        GetConsoleSelectionInfo,
        GetConsoleTitle,
        GetConsoleWindow,
        GetCurrentConsoleFontEx,
        GetLargestConsoleWindowSize,
        GetNumberOfConsoleInputEvents,
        GetNumberOfConsoleMouseButtons,
        PeekConsoleInput,
        ReadConsole,
        ReadConsoleInput,
        ReadConsoleOutput,
        ReadConsoleOutputAttribute,
        ReadConsoleOutputCharacter,
        ScrollConsoleScreenBuffer,
        SetConsoleActiveScreenBuffer,
        SetConsoleCP,
        SetConsoleCursorInfo,
        SetConsoleCursorPosition,
        SetConsoleDisplayMode,
        SetConsoleHistoryInfo,
        SetConsoleMode,
        SetConsoleOutputCP,
        SetConsoleScreenBufferInfoEx,
        SetConsoleScreenBufferSize,
        SetConsoleTextAttribute,
        SetConsoleTitle,
        SetConsoleWindowInfo,
        SetCurrentConsoleFontEx,
        WriteConsole,
        WriteConsoleInput,
        WriteConsoleOutput,
        WriteConsoleOutputAttribute,
        WriteConsoleOutputCharacter,
        // Only use this last enum as a count of the number of api enums.
        NUMBER_OF_APIS
    };
    void LogApiCall(const ApiCall api);
    void LogApiCall(const ApiCall api, const BOOLEAN fUnicode);

private:
    // Used to prevent multiple instances
    Telemetry();
    ~Telemetry();
    Telemetry(Telemetry const&);
    void operator=(Telemetry const&);

    bool FindProcessName(const WCHAR* pszProcessName, _Out_ size_t* iPosition) const;
    void TotalCodesForPreviousProcess();

    static const int c_iMaxProcessesConnected = 100;

    TraceLoggingActivity<g_hConhostV2EventTraceProvider> _activity;

    float _fpFindStringLengthAverage;
    float _fpDirectionDownAverage;
    float _fpMatchCaseAverage;
    unsigned int _uiFindNextClickedTotal;
    unsigned int _uiColorSelectionUsed;
    time_t _tStartedAt;
    WCHAR const* const c_pwszBashExeName = L"bash.exe";

    // The current recommendation is to keep telemetry events 4KB or less, so let's keep our array at less than 2KB (1000 * 2 bytes).
    WCHAR _wchProcessFileNames[1000];
    // Index into our specially packed string, where to insert the next string.
    size_t _iProcessFileNamesNext;
    // Index for the currently connected process.
    size_t _iProcessConnectedCurrently;
    // An array of indexes into the _wchProcessFileNames array, which point to the individual process names.
    size_t _rgiProccessFileNameIndex[c_iMaxProcessesConnected];
    // Number of times each process has connected to the console.
    unsigned int _rguiProcessFileNamesCount[c_iMaxProcessesConnected];
    // To speed up searching the Process Names, create an alphabetically sorted index.
    size_t _rgiAlphabeticalIndex[c_iMaxProcessesConnected];
    // Total of how many codes each process used
    unsigned int _rguiProcessFileNamesCodesCount[c_iMaxProcessesConnected];
    // Total of how many failed codes each process used
    unsigned int _rguiProcessFileNamesFailedCodesCount[c_iMaxProcessesConnected];
    // Total of how many failed codes each process used outside the valid range.
    unsigned int _rguiProcessFileNamesFailedOutsideCodesCount[c_iMaxProcessesConnected];
    unsigned int _rguiTimesApiUsed[NUMBER_OF_APIS];
    // Most of this array will be empty, and is only used if an API has an ansi specific variant.
    unsigned int _rguiTimesApiUsedAnsi[NUMBER_OF_APIS];
    // Total number of file names we've added.
    UINT16 _uiNumberProcessFileNames;

    bool _fBashUsed;
    bool _fKeyboardTextEditingUsed;
    bool _fKeyboardTextSelectionUsed;
    bool _fUserInteractiveForTelemetry;
    bool _fCtrlPgUpPgDnUsed;

    // Linux copy and paste keyboard shortcut telemetry
    unsigned int _uiCtrlShiftCProcUsed;
    unsigned int _uiCtrlShiftCRawUsed;
    unsigned int _uiCtrlShiftVProcUsed;
    unsigned int _uiCtrlShiftVRawUsed;

    // Quick edit copy and paste usage telemetry
    unsigned int _uiQuickEditCopyProcUsed;
    unsigned int _uiQuickEditCopyRawUsed;
    unsigned int _uiQuickEditPasteProcUsed;
    unsigned int _uiQuickEditPasteRawUsed;
};

// Log the RIPMSG through telemetry, and also through a normal OutputDebugStringW call.
// These are drop-in substitutes for the RIPMSG0-4 macros from \windows\Core\ntcon2\conhost\consrv.h
#define RIPMSG0(flags, msg) Telemetry::Instance().LogRipMessage(msg);
#define RIPMSG1(flags, msg, a) Telemetry::Instance().LogRipMessage(msg, a);
#define RIPMSG2(flags, msg, a, b) Telemetry::Instance().LogRipMessage(msg, a, b);
#define RIPMSG3(flags, msg, a, b, c) Telemetry::Instance().LogRipMessage(msg, a, b, c);
#define RIPMSG4(flags, msg, a, b, c, d) Telemetry::Instance().LogRipMessage(msg, a, b, c, d);
