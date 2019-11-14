// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"
#include "OutputStateMachineEngine.hpp"

#include "ascii.hpp"
using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;

// takes ownership of pDispatch
OutputStateMachineEngine::OutputStateMachineEngine(ITermDispatch* const pDispatch) :
    _dispatch(pDispatch),
    _pfnFlushToTerminal(nullptr),
    _pTtyConnection(nullptr),
    _lastPrintedChar(AsciiChars::NUL)
{
}

OutputStateMachineEngine::~OutputStateMachineEngine()
{
}

const ITermDispatch& OutputStateMachineEngine::Dispatch() const noexcept
{
    return *_dispatch;
}

ITermDispatch& OutputStateMachineEngine::Dispatch() noexcept
{
    return *_dispatch;
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionExecute(const wchar_t wch)
{
    // microsoft/terminal#1825 - VT applications expect to be able to write NUL
    // and have _nothing_ happen. Filter the NULs here, so they don't fill the
    // buffer with empty spaces.
    if (wch == AsciiChars::NUL)
    {
        return true;
    }

    _dispatch->Execute(wch);
    _ClearLastChar();

    if (wch == AsciiChars::BEL)
    {
        // microsoft/terminal#2952
        // If we're attached to a terminal, let's also pass the BEL through.
        if (_pfnFlushToTerminal != nullptr)
        {
            _pfnFlushToTerminal();
        }
    }

    return true;
}

// Routine Description:
// - Triggers the Execute action to indicate that the listener should
//      immediately respond to a C0 control character.
// This is called from the Escape state in the state machine, indicating the
//      immediately previous character was an 0x1b. The output state machine
//      does not treat this any differently than a normal ActionExecute.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionExecuteFromEscape(const wchar_t wch)
{
    return ActionExecute(wch);
}

// Routine Description:
// - Triggers the Print action to indicate that the listener should render the
//      character given.
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPrint(const wchar_t wch)
{
    // Stash the last character of the string, if it's a graphical character
    if (wch >= AsciiChars::SPC)
    {
        _lastPrintedChar = wch;
    }

    _dispatch->Print(wch); // call print

    return true;
}

// Routine Description:
// - Triggers the Print action to indicate that the listener should render the
//      string of characters given.
// Arguments:
// - rgwch - string to dispatch.
// - cch - length of rgwch
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPrintString(const wchar_t* const rgwch, const size_t cch)
{
    if (cch == 0)
    {
        return true;
    }
    // Stash the last character of the string, if it's a graphical character
    const wchar_t wch = rgwch[cch - 1];
    if (wch >= AsciiChars::SPC)
    {
        _lastPrintedChar = wch;
    }

    _dispatch->PrintString(rgwch, cch); // call print

    return true;
}

// Routine Description:
// This is called when we have determined that we don't understand a particular
//      sequence, or the adapter has determined that the string is intended for
//      the actual terminal (when we're acting as a pty).
// - Pass the string through to the target terminal application. If we're a pty,
//      then we'll have a TerminalConnection that we'll write the string to.
//      Otherwise, we're the terminal device, and we'll eat the string (because
//      we don't know what to do with it)
// Arguments:
// - rgwch - string to dispatch.
// - cch - length of rgwch
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPassThroughString(const wchar_t* const rgwch,
                                                       _In_ size_t const cch)
{
    bool fSuccess = true;
    if (_pTtyConnection != nullptr)
    {
        std::wstring wstr = std::wstring(rgwch, cch);
        auto hr = _pTtyConnection->WriteTerminalW(wstr);
        LOG_IF_FAILED(hr);
        fSuccess = SUCCEEDED(hr);
    }
    // If there's not a TTY connection, our previous behavior was to eat the string.

    return fSuccess;
}

// Routine Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - wch - Character to dispatch.
// - cIntermediate - Number of "Intermediate" characters found - such as '!', '?'
// - wchIntermediate - Intermediate character in the sequence, if there was one.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionEscDispatch(const wchar_t wch,
                                                 const unsigned short cIntermediate,
                                                 const wchar_t wchIntermediate)
{
    bool fSuccess = false;

    // no intermediates.
    if (cIntermediate == 0)
    {
        switch (wch)
        {
        case VTActionCodes::CUU_CursorUp:
            fSuccess = _dispatch->CursorUp(1);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::CUU);
            break;
        case VTActionCodes::CUD_CursorDown:
            fSuccess = _dispatch->CursorDown(1);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::CUD);
            break;
        case VTActionCodes::CUF_CursorForward:
            fSuccess = _dispatch->CursorForward(1);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::CUF);
            break;
        case VTActionCodes::CUB_CursorBackward:
            fSuccess = _dispatch->CursorBackward(1);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::CUB);
            break;
        case VTActionCodes::DECSC_CursorSave:
            fSuccess = _dispatch->CursorSaveState();
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSC);
            break;
        case VTActionCodes::DECRC_CursorRestore:
            fSuccess = _dispatch->CursorRestoreState();
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECRC);
            break;
        case VTActionCodes::DECKPAM_KeypadApplicationMode:
            fSuccess = _dispatch->SetKeypadMode(true);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECKPAM);
            break;
        case VTActionCodes::DECKPNM_KeypadNumericMode:
            fSuccess = _dispatch->SetKeypadMode(false);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECKPNM);
            break;
        case VTActionCodes::RI_ReverseLineFeed:
            fSuccess = _dispatch->ReverseLineFeed();
            TermTelemetry::Instance().Log(TermTelemetry::Codes::RI);
            break;
        case VTActionCodes::HTS_HorizontalTabSet:
            fSuccess = _dispatch->HorizontalTabSet();
            TermTelemetry::Instance().Log(TermTelemetry::Codes::HTS);
            break;
        case VTActionCodes::RIS_ResetToInitialState:
            fSuccess = _dispatch->HardReset();
            TermTelemetry::Instance().Log(TermTelemetry::Codes::RIS);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
    }
    else if (cIntermediate == 1)
    {
        DesignateCharsetTypes designateType = s_DefaultDesignateCharsetType;
        fSuccess = _GetDesignateType(wchIntermediate, &designateType);
        if (fSuccess)
        {
            switch (designateType)
            {
            case DesignateCharsetTypes::G0:
                fSuccess = _dispatch->DesignateCharset(wch);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG0);
                break;
            case DesignateCharsetTypes::G1:
                fSuccess = false;
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG1);
                break;
            case DesignateCharsetTypes::G2:
                fSuccess = false;
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG2);
                break;
            case DesignateCharsetTypes::G3:
                fSuccess = false;
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG3);
                break;
            default:
                // If no functions to call, overall dispatch was a failure.
                fSuccess = false;
                break;
            }
        }
    }

    _ClearLastChar();

    return fSuccess;
}

// Routine Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - cIntermediate - Number of "Intermediate" characters found - such as '!', '?'
// - wchIntermediate - Intermediate character in the sequence, if there was one.
// - rgusParams - set of numeric parameters collected while pasring the sequence.
// - cParams - number of parameters found.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionCsiDispatch(const wchar_t wch,
                                                 const unsigned short cIntermediate,
                                                 const wchar_t wchIntermediate,
                                                 _In_reads_(cParams) const unsigned short* const rgusParams,
                                                 const unsigned short cParams)
{
    bool fSuccess = false;
    unsigned int uiDistance = 0;
    unsigned int uiLine = 0;
    unsigned int uiColumn = 0;
    SHORT sTopMargin = 0;
    SHORT sBottomMargin = 0;
    SHORT sNumTabs = 0;
    SHORT sClearType = 0;
    unsigned int uiFunction = 0;
    DispatchTypes::EraseType eraseType = DispatchTypes::EraseType::ToEnd;
    DispatchTypes::GraphicsOptions rgGraphicsOptions[StateMachine::s_cParamsMax];
    size_t cOptions = StateMachine::s_cParamsMax;
    DispatchTypes::AnsiStatusType deviceStatusType = (DispatchTypes::AnsiStatusType)-1; // there is no default status type.
    unsigned int repeatCount = 0;
    // This is all the args after the first arg, and the count of args not including the first one.
    const unsigned short* const rgusRemainingArgs = (cParams > 1) ? rgusParams + 1 : rgusParams;
    const unsigned short cRemainingArgs = (cParams >= 1) ? cParams - 1 : 0;

    if (cIntermediate == 0)
    {
        // fill params
        switch (wch)
        {
        case VTActionCodes::CUU_CursorUp:
        case VTActionCodes::CUD_CursorDown:
        case VTActionCodes::CUF_CursorForward:
        case VTActionCodes::CUB_CursorBackward:
        case VTActionCodes::CNL_CursorNextLine:
        case VTActionCodes::CPL_CursorPrevLine:
        case VTActionCodes::CHA_CursorHorizontalAbsolute:
        case VTActionCodes::HPA_HorizontalPositionAbsolute:
        case VTActionCodes::VPA_VerticalLinePositionAbsolute:
        case VTActionCodes::ICH_InsertCharacter:
        case VTActionCodes::DCH_DeleteCharacter:
        case VTActionCodes::ECH_EraseCharacters:
            fSuccess = _GetCursorDistance(rgusParams, cParams, &uiDistance);
            break;
        case VTActionCodes::HVP_HorizontalVerticalPosition:
        case VTActionCodes::CUP_CursorPosition:
            fSuccess = _GetXYPosition(rgusParams, cParams, &uiLine, &uiColumn);
            break;
        case VTActionCodes::DECSTBM_SetScrollingRegion:
            fSuccess = _GetTopBottomMargins(rgusParams, cParams, &sTopMargin, &sBottomMargin);
            break;
        case VTActionCodes::ED_EraseDisplay:
        case VTActionCodes::EL_EraseLine:
            fSuccess = _GetEraseOperation(rgusParams, cParams, &eraseType);
            break;
        case VTActionCodes::SGR_SetGraphicsRendition:
            fSuccess = _GetGraphicsOptions(rgusParams, cParams, rgGraphicsOptions, &cOptions);
            break;
        case VTActionCodes::DSR_DeviceStatusReport:
            fSuccess = _GetDeviceStatusOperation(rgusParams, cParams, &deviceStatusType);
            break;
        case VTActionCodes::DA_DeviceAttributes:
            fSuccess = _VerifyDeviceAttributesParams(rgusParams, cParams);
            break;
        case VTActionCodes::SU_ScrollUp:
        case VTActionCodes::SD_ScrollDown:
            fSuccess = _GetScrollDistance(rgusParams, cParams, &uiDistance);
            break;
        case VTActionCodes::ANSISYSSC_CursorSave:
        case VTActionCodes::ANSISYSRC_CursorRestore:
            fSuccess = _VerifyHasNoParameters(cParams);
            break;
        case VTActionCodes::IL_InsertLine:
        case VTActionCodes::DL_DeleteLine:
            fSuccess = _GetScrollDistance(rgusParams, cParams, &uiDistance);
            break;
        case VTActionCodes::CHT_CursorForwardTab:
        case VTActionCodes::CBT_CursorBackTab:
            fSuccess = _GetTabDistance(rgusParams, cParams, &sNumTabs);
            break;
        case VTActionCodes::TBC_TabClear:
            fSuccess = _GetTabClearType(rgusParams, cParams, &sClearType);
            break;
        case VTActionCodes::DTTERM_WindowManipulation:
            fSuccess = _GetWindowManipulationType(rgusParams, cParams, &uiFunction);
            break;
        case VTActionCodes::REP_RepeatCharacter:
            fSuccess = _GetRepeatCount(rgusParams, cParams, &repeatCount);
            break;
        default:
            // If no params to fill, param filling was successful.
            fSuccess = true;
            break;
        }

        // if param filling successful, try to dispatch
        if (fSuccess)
        {
            switch (wch)
            {
            case VTActionCodes::CUU_CursorUp:
                fSuccess = _dispatch->CursorUp(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CUU);
                break;
            case VTActionCodes::CUD_CursorDown:
                fSuccess = _dispatch->CursorDown(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CUD);
                break;
            case VTActionCodes::CUF_CursorForward:
                fSuccess = _dispatch->CursorForward(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CUF);
                break;
            case VTActionCodes::CUB_CursorBackward:
                fSuccess = _dispatch->CursorBackward(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CUB);
                break;
            case VTActionCodes::CNL_CursorNextLine:
                fSuccess = _dispatch->CursorNextLine(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CNL);
                break;
            case VTActionCodes::CPL_CursorPrevLine:
                fSuccess = _dispatch->CursorPrevLine(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CPL);
                break;
            case VTActionCodes::CHA_CursorHorizontalAbsolute:
            case VTActionCodes::HPA_HorizontalPositionAbsolute:
                fSuccess = _dispatch->CursorHorizontalPositionAbsolute(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CHA);
                break;
            case VTActionCodes::VPA_VerticalLinePositionAbsolute:
                fSuccess = _dispatch->VerticalLinePositionAbsolute(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::VPA);
                break;
            case VTActionCodes::CUP_CursorPosition:
            case VTActionCodes::HVP_HorizontalVerticalPosition:
                fSuccess = _dispatch->CursorPosition(uiLine, uiColumn);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CUP);
                break;
            case VTActionCodes::DECSTBM_SetScrollingRegion:
                fSuccess = _dispatch->SetTopBottomScrollingMargins(sTopMargin, sBottomMargin);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSTBM);
                break;
            case VTActionCodes::ICH_InsertCharacter:
                fSuccess = _dispatch->InsertCharacter(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::ICH);
                break;
            case VTActionCodes::DCH_DeleteCharacter:
                fSuccess = _dispatch->DeleteCharacter(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DCH);
                break;
            case VTActionCodes::ED_EraseDisplay:
                fSuccess = _dispatch->EraseInDisplay(eraseType);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::ED);
                break;
            case VTActionCodes::EL_EraseLine:
                fSuccess = _dispatch->EraseInLine(eraseType);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::EL);
                break;
            case VTActionCodes::SGR_SetGraphicsRendition:
                fSuccess = _dispatch->SetGraphicsRendition(rgGraphicsOptions, cOptions);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::SGR);
                break;
            case VTActionCodes::DSR_DeviceStatusReport:
                fSuccess = _dispatch->DeviceStatusReport(deviceStatusType);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DSR);
                break;
            case VTActionCodes::DA_DeviceAttributes:
                fSuccess = _dispatch->DeviceAttributes();
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DA);
                break;
            case VTActionCodes::SU_ScrollUp:
                fSuccess = _dispatch->ScrollUp(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::SU);
                break;
            case VTActionCodes::SD_ScrollDown:
                fSuccess = _dispatch->ScrollDown(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::SD);
                break;
            case VTActionCodes::ANSISYSSC_CursorSave:
                fSuccess = _dispatch->CursorSaveState();
                TermTelemetry::Instance().Log(TermTelemetry::Codes::ANSISYSSC);
                break;
            case VTActionCodes::ANSISYSRC_CursorRestore:
                fSuccess = _dispatch->CursorRestoreState();
                TermTelemetry::Instance().Log(TermTelemetry::Codes::ANSISYSRC);
                break;
            case VTActionCodes::IL_InsertLine:
                fSuccess = _dispatch->InsertLine(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::IL);
                break;
            case VTActionCodes::DL_DeleteLine:
                fSuccess = _dispatch->DeleteLine(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DL);
                break;
            case VTActionCodes::CHT_CursorForwardTab:
                fSuccess = _dispatch->ForwardTab(sNumTabs);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CHT);
                break;
            case VTActionCodes::CBT_CursorBackTab:
                fSuccess = _dispatch->BackwardsTab(sNumTabs);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::CBT);
                break;
            case VTActionCodes::TBC_TabClear:
                fSuccess = _dispatch->TabClear(sClearType);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::TBC);
                break;
            case VTActionCodes::ECH_EraseCharacters:
                fSuccess = _dispatch->EraseCharacters(uiDistance);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::ECH);
                break;
            case VTActionCodes::DTTERM_WindowManipulation:
                fSuccess = _dispatch->WindowManipulation(static_cast<DispatchTypes::WindowManipulationType>(uiFunction),
                                                         rgusRemainingArgs,
                                                         cRemainingArgs);
                TermTelemetry::Instance().Log(TermTelemetry::Codes::DTTERM_WM);
                break;
            case VTActionCodes::REP_RepeatCharacter:
                // Handled w/o the dispatch. This function is unique in that way
                // If this were in the ITerminalDispatch, then each
                // implementation would effectively be the same, calling only
                // functions that are already part of the interface.
                // Print the last graphical character a number of times.
                if (_lastPrintedChar != AsciiChars::NUL)
                {
                    std::wstring wstr(repeatCount, _lastPrintedChar);
                    _dispatch->PrintString(wstr.c_str(), wstr.length());
                }
                fSuccess = true;
                TermTelemetry::Instance().Log(TermTelemetry::Codes::REP);
                break;
            default:
                // If no functions to call, overall dispatch was a failure.
                fSuccess = false;
                break;
            }
        }
    }
    else if (cIntermediate == 1)
    {
        switch (wchIntermediate)
        {
        case L'?':
            fSuccess = _IntermediateQuestionMarkDispatch(wch, rgusParams, cParams);
            break;
        case L'!':
            fSuccess = _IntermediateExclamationDispatch(wch);
            break;
        case L' ':
            fSuccess = _IntermediateSpaceDispatch(wch, rgusParams, cParams);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
    }
    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !fSuccess)
    {
        fSuccess = _pfnFlushToTerminal();
    }

    _ClearLastChar();

    return fSuccess;
}

// Routine Description:
// - Handles actions that have postfix params on an intermediate '?', such as DECTCEM, DECCOLM, ATT610
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - True if handled successfully. False otherwise.
bool OutputStateMachineEngine::_IntermediateQuestionMarkDispatch(const wchar_t wchAction,
                                                                 _In_reads_(cParams) const unsigned short* const rgusParams,
                                                                 const unsigned short cParams)
{
    bool fSuccess = false;

    DispatchTypes::PrivateModeParams rgPrivateModeParams[StateMachine::s_cParamsMax];
    size_t cOptions = StateMachine::s_cParamsMax;
    // Ensure that there was the right number of params
    switch (wchAction)
    {
    case VTActionCodes::DECSET_PrivateModeSet:
    case VTActionCodes::DECRST_PrivateModeReset:
        fSuccess = _GetPrivateModeParams(rgusParams, cParams, rgPrivateModeParams, &cOptions);
        break;

    default:
        // If no params to fill, param filling was successful.
        fSuccess = true;
        break;
    }
    if (fSuccess)
    {
        switch (wchAction)
        {
        case VTActionCodes::DECSET_PrivateModeSet:
            fSuccess = _dispatch->SetPrivateModes(rgPrivateModeParams, cOptions);
            //TODO: MSFT:6367459 Add specific logging for each of the DECSET/DECRST codes
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSET);
            break;
        case VTActionCodes::DECRST_PrivateModeReset:
            fSuccess = _dispatch->ResetPrivateModes(rgPrivateModeParams, cOptions);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECRST);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
    }
    return fSuccess;
}

// Routine Description:
// - Handles actions that have an intermediate '!', such as DECSTR
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - True if handled successfully. False otherwise.
bool OutputStateMachineEngine::_IntermediateExclamationDispatch(const wchar_t wchAction)
{
    bool fSuccess = false;

    switch (wchAction)
    {
    case VTActionCodes::DECSTR_SoftReset:
        fSuccess = _dispatch->SoftReset();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSTR);
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        fSuccess = false;
        break;
    }
    return fSuccess;
}

// Routine Description:
// - Handles actions that have an intermediate ' ' (0x20), such as DECSCUSR
// Arguments:
// - wch - Character to dispatch.
// Return Value:
// - True if handled successfully. False otherwise.
bool OutputStateMachineEngine::_IntermediateSpaceDispatch(const wchar_t wchAction,
                                                          _In_reads_(cParams) const unsigned short* const rgusParams,
                                                          const unsigned short cParams)
{
    bool fSuccess = false;
    DispatchTypes::CursorStyle cursorStyle = s_defaultCursorStyle;

    // Parse params
    switch (wchAction)
    {
    case VTActionCodes::DECSCUSR_SetCursorStyle:
        fSuccess = _GetCursorStyle(rgusParams, cParams, &cursorStyle);
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        fSuccess = false;
        break;
    }

    // if param filling successful, try to dispatch
    if (fSuccess)
    {
        switch (wchAction)
        {
        case VTActionCodes::DECSCUSR_SetCursorStyle:
            fSuccess = _dispatch->SetCursorStyle(cursorStyle);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSCUSR);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Triggers the Clear action to indicate that the state machine should erase
//      all internal state.
// Arguments:
// - <none>
// Return Value:
// - <none>
bool OutputStateMachineEngine::ActionClear()
{
    // do nothing.
    return true;
}

// Routine Description:
// - Triggers the Ignore action to indicate that the state machine should eat
//      this character and say nothing.
// Arguments:
// - <none>
// Return Value:
// - <none>
bool OutputStateMachineEngine::ActionIgnore()
{
    // do nothing.
    return true;
}

// Routine Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch. This will be a BEL or ST char.
// - sOscParam - identifier of the OSC action to perform
// - pwchOscStringBuffer - OSC string we've collected. NOT null terminated.
// - cchOscString - length of pwchOscStringBuffer
// Return Value:
// - true if we handled the dsipatch.
bool OutputStateMachineEngine::ActionOscDispatch(const wchar_t /*wch*/,
                                                 const unsigned short sOscParam,
                                                 _Inout_updates_(cchOscString) wchar_t* const pwchOscStringBuffer,
                                                 const unsigned short cchOscString)
{
    bool fSuccess = false;
    wchar_t* pwchTitle = nullptr;
    unsigned short sCchTitleLength = 0;
    size_t tableIndex = 0;
    DWORD dwColor = 0;

    switch (sOscParam)
    {
    case OscActionCodes::SetIconAndWindowTitle:
    case OscActionCodes::SetWindowIcon:
    case OscActionCodes::SetWindowTitle:
        fSuccess = _GetOscTitle(pwchOscStringBuffer, cchOscString, &pwchTitle, &sCchTitleLength);
        break;
    case OscActionCodes::SetColor:
        fSuccess = _GetOscSetColorTable(pwchOscStringBuffer, cchOscString, &tableIndex, &dwColor);
        break;
    case OscActionCodes::SetForegroundColor:
    case OscActionCodes::SetBackgroundColor:
    case OscActionCodes::SetCursorColor:
        fSuccess = _GetOscSetColor(pwchOscStringBuffer, cchOscString, &dwColor);
        break;
    case OscActionCodes::ResetCursorColor:
        // the console uses 0xffffffff as an "invalid color" value
        dwColor = 0xffffffff;
        fSuccess = true;
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        fSuccess = false;
        break;
    }
    if (fSuccess)
    {
        switch (sOscParam)
        {
        case OscActionCodes::SetIconAndWindowTitle:
        case OscActionCodes::SetWindowIcon:
        case OscActionCodes::SetWindowTitle:
            fSuccess = _dispatch->SetWindowTitle({ pwchTitle, sCchTitleLength });
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCWT);
            break;
        case OscActionCodes::SetColor:
            fSuccess = _dispatch->SetColorTableEntry(tableIndex, dwColor);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCCT);
            break;
        case OscActionCodes::SetForegroundColor:
            fSuccess = _dispatch->SetDefaultForeground(dwColor);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCFG);
            break;
        case OscActionCodes::SetBackgroundColor:
            fSuccess = _dispatch->SetDefaultBackground(dwColor);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCBG);
            break;
        case OscActionCodes::SetCursorColor:
            fSuccess = _dispatch->SetCursorColor(dwColor);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCSCC);
            break;
        case OscActionCodes::ResetCursorColor:
            fSuccess = _dispatch->SetCursorColor(dwColor);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCRCC);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            fSuccess = false;
            break;
        }
    }

    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !fSuccess)
    {
        fSuccess = _pfnFlushToTerminal();
    }

    _ClearLastChar();

    return fSuccess;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - rgusParams - set of numeric parameters collected while pasring the sequence.
// - cParams - number of parameters found.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionSs3Dispatch(const wchar_t /*wch*/,
                                                 _In_reads_(_Param_(3)) const unsigned short* const /*rgusParams*/,
                                                 const unsigned short /*cParams*/)
{
    // The output engine doesn't handle any SS3 sequences.
    _ClearLastChar();
    return false;
}

// Routine Description:
// - Retrieves the listed graphics options to be applied in order to the "font style" of the next characters inserted into the buffer.
// Arguments:
// - rgGraphicsOptions - Pointer to array space (expected 16 max, the max number of params this can generate) that will be filled with valid options from the GraphicsOptions enum
// - pcOptions - Pointer to the length of rgGraphicsOptions on the way in, and the count of the array used on the way out.
// Return Value:
// - True if we successfully retrieved an array of valid graphics options from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetGraphicsOptions(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                      const unsigned short cParams,
                                                                      _Out_writes_(*pcOptions) DispatchTypes::GraphicsOptions* const rgGraphicsOptions,
                                                                      _Inout_ size_t* const pcOptions) const
{
    bool fSuccess = false;

    if (cParams == 0)
    {
        if (*pcOptions >= 1)
        {
            rgGraphicsOptions[0] = s_defaultGraphicsOption;
            *pcOptions = 1;
            fSuccess = true;
        }
        else
        {
            fSuccess = false; // not enough space in buffer to hold response.
        }
    }
    else
    {
        if (*pcOptions >= cParams)
        {
            for (size_t i = 0; i < cParams; i++)
            {
                // No memcpy. The parameters are shorts. The graphics options are unsigned ints.
                rgGraphicsOptions[i] = (DispatchTypes::GraphicsOptions)rgusParams[i];
            }

            *pcOptions = cParams;
            fSuccess = true;
        }
        else
        {
            fSuccess = false; // not enough space in buffer to hold response.
        }
    }

    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !fSuccess)
    {
        fSuccess = _pfnFlushToTerminal();
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves the erase type parameter for an upcoming operation.
// Arguments:
// - pEraseType - Memory location to receive the erase type parameter
// Return Value:
// - True if we successfully pulled an erase type from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetEraseOperation(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                     const unsigned short cParams,
                                                                     _Out_ DispatchTypes::EraseType* const pEraseType) const
{
    bool fSuccess = false; // If we have too many parameters or don't know what to do with the given value, return false.
    *pEraseType = s_defaultEraseType; // if we fail, just put the default type in.

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        *pEraseType = s_defaultEraseType;
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, attempt to match it to the values we accept.
        unsigned short const usParam = rgusParams[0];

        switch (static_cast<DispatchTypes::EraseType>(usParam))
        {
        case DispatchTypes::EraseType::ToEnd:
        case DispatchTypes::EraseType::FromBeginning:
        case DispatchTypes::EraseType::All:
        case DispatchTypes::EraseType::Scrollback:
            *pEraseType = (DispatchTypes::EraseType)usParam;
            fSuccess = true;
            break;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves a distance for a cursor operation from the parameter pool stored during Param actions.
// Arguments:
// - puiDistance - Memory location to receive the distance
// Return Value:
// - True if we successfully pulled the cursor distance from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetCursorDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                     const unsigned short cParams,
                                                                     _Out_ unsigned int* const puiDistance) const
{
    bool fSuccess = false;
    *puiDistance = s_uiDefaultCursorDistance;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *puiDistance = rgusParams[0];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 1.
    if (*puiDistance == 0)
    {
        *puiDistance = s_uiDefaultCursorDistance;
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves a distance for a scroll operation from the parameter pool stored during Param actions.
// Arguments:
// - puiDistance - Memory location to receive the distance
// Return Value:
// - True if we successfully pulled the scroll distance from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetScrollDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                     const unsigned short cParams,
                                                                     _Out_ unsigned int* const puiDistance) const
{
    bool fSuccess = false;
    *puiDistance = s_uiDefaultScrollDistance;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *puiDistance = rgusParams[0];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 1.
    if (*puiDistance == 0)
    {
        *puiDistance = s_uiDefaultScrollDistance;
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves a width for the console window from the parameter pool stored during Param actions.
// Arguments:
// - puiConsoleWidth - Memory location to receive the width
// Return Value:
// - True if we successfully pulled the width from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetConsoleWidth(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                   const unsigned short cParams,
                                                                   _Out_ unsigned int* const puiConsoleWidth) const
{
    bool fSuccess = false;
    *puiConsoleWidth = s_uiDefaultConsoleWidth;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *puiConsoleWidth = rgusParams[0];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 80.
    if (*puiConsoleWidth == 0)
    {
        *puiConsoleWidth = s_uiDefaultConsoleWidth;
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves an X/Y coordinate pair for a cursor operation from the parameter pool stored during Param actions.
// Arguments:
// - puiLine - Memory location to receive the Y/Line/Row position
// - puiColumn - Memory location to receive the X/Column position
// Return Value:
// - True if we successfully pulled the cursor coordinates from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetXYPosition(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                 const unsigned short cParams,
                                                                 _Out_ unsigned int* const puiLine,
                                                                 _Out_ unsigned int* const puiColumn) const
{
    bool fSuccess = false;
    *puiLine = s_uiDefaultLine;
    *puiColumn = s_uiDefaultColumn;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's only one param, leave the default for the column, and retrieve the specified row.
        *puiLine = rgusParams[0];
        fSuccess = true;
    }
    else if (cParams == 2)
    {
        // If there are exactly two parameters, use them.
        *puiLine = rgusParams[0];
        *puiColumn = rgusParams[1];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 1.
    if (*puiLine == 0)
    {
        *puiLine = s_uiDefaultLine;
    }

    if (*puiColumn == 0)
    {
        *puiColumn = s_uiDefaultColumn;
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves a top and bottom pair for setting the margins from the parameter pool stored during Param actions
// Arguments:
// - psTopMargin - Memory location to receive the top margin
// - psBottomMargin - Memory location to receive the bottom margin
// Return Value:
// - True if we successfully pulled the margin settings from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetTopBottomMargins(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                       const unsigned short cParams,
                                                                       _Out_ SHORT* const psTopMargin,
                                                                       _Out_ SHORT* const psBottomMargin) const
{
    // Notes:                           (input -> state machine out)
    // having only a top param is legal         ([3;r   -> 3,0)
    // having only a bottom param is legal      ([;3r   -> 0,3)
    // having neither uses the defaults         ([;r [r -> 0,0)
    // an illegal combo (eg, 3;2r) is ignored

    bool fSuccess = false;
    *psTopMargin = s_sDefaultTopMargin;
    *psBottomMargin = s_sDefaultBottomMargin;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        *psTopMargin = rgusParams[0];
        fSuccess = true;
    }
    else if (cParams == 2)
    {
        // If there are exactly two parameters, use them.
        *psTopMargin = rgusParams[0];
        *psBottomMargin = rgusParams[1];
        fSuccess = true;
    }

    if (*psBottomMargin > 0 && *psBottomMargin < *psTopMargin)
    {
        fSuccess = false;
    }
    return fSuccess;
}
// Routine Description:
// - Retrieves the status type parameter for an upcoming device query operation
// Arguments:
// - pStatusType - Memory location to receive the Status Type parameter
// Return Value:
// - True if we successfully found a device operation in the parameters stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetDeviceStatusOperation(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                            const unsigned short cParams,
                                                                            _Out_ DispatchTypes::AnsiStatusType* const pStatusType) const
{
    bool fSuccess = false;
    *pStatusType = (DispatchTypes::AnsiStatusType)0;

    if (cParams == 1)
    {
        // If there's one parameter, attempt to match it to the values we accept.
        unsigned short const usParam = rgusParams[0];

        switch (usParam)
        {
        // This looks kinda silly, but I want the parser to reject (fSuccess = false) any status types we haven't put here.
        case (unsigned short)DispatchTypes::AnsiStatusType::CPR_CursorPositionReport:
            *pStatusType = DispatchTypes::AnsiStatusType::CPR_CursorPositionReport;
            fSuccess = true;
            break;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves the listed private mode params be set/reset by DECSET/DECRST
// Arguments:
// - rPrivateModeParams - Pointer to array space (expected 16 max, the max number of params this can generate) that will be filled with valid params from the PrivateModeParams enum
// - pcParams - Pointer to the length of rPrivateModeParams on the way in, and the count of the array used on the way out.
// Return Value:
// - True if we successfully retrieved an array of private mode params from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetPrivateModeParams(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                        const unsigned short cParams,
                                                                        _Out_writes_(*pcParams) DispatchTypes::PrivateModeParams* const rgPrivateModeParams,
                                                                        _Inout_ size_t* const pcParams) const
{
    bool fSuccess = false;
    // Can't just set nothing at all
    if (cParams > 0)
    {
        if (*pcParams >= cParams)
        {
            for (size_t i = 0; i < cParams; i++)
            {
                // No memcpy. The parameters are shorts. The graphics options are unsigned ints.
                rgPrivateModeParams[i] = (DispatchTypes::PrivateModeParams)rgusParams[i];
            }
            *pcParams = cParams;
            fSuccess = true;
        }
        else
        {
            fSuccess = false; // not enough space in buffer to hold response.
        }
    }
    return fSuccess;
}

// - Verifies that no parameters were parsed for the current CSI sequence
// Arguments:
// - <none>
// Return Value:
// - True if there were no parameters. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_VerifyHasNoParameters(const unsigned short cParams) const
{
    return cParams == 0;
}

// Routine Description:
// - Validates that we received the correct parameter sequence for the Device Attributes command.
// - For DA, we should have received either NO parameters or just one 0 parameter. Anything else is not acceptable.
// Arguments:
// - <none>
// Return Value:
// - True if the DA params were valid. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_VerifyDeviceAttributesParams(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                                const unsigned short cParams) const
{
    bool fSuccess = false;

    if (cParams == 0)
    {
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        if (rgusParams[0] == 0)
        {
            fSuccess = true;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Null terminates, then returns, the string that we've collected as part of the OSC string.
// Arguments:
// - ppwchTitle - a pointer to point to the Osc String to use as a title.
// - pcchTitle  - a pointer place the length of ppwchTitle into.
// Return Value:
// - True if there was a title to output. (a title with length=0 is still valid)
_Success_(return ) bool OutputStateMachineEngine::_GetOscTitle(_Inout_updates_(cchOscString) wchar_t* const pwchOscStringBuffer,
                                                               const unsigned short cchOscString,
                                                               _Outptr_result_buffer_(*pcchTitle) wchar_t** const ppwchTitle,
                                                               _Out_ unsigned short* pcchTitle) const
{
    *ppwchTitle = pwchOscStringBuffer;
    *pcchTitle = cchOscString;

    return pwchOscStringBuffer != nullptr;
}

// Routine Description:
// - Retrieves a distance for a tab operation from the parameter pool stored during Param actions.
// Arguments:
// - psDistance - Memory location to receive the distance
// Return Value:
// - True if we successfully pulled the tab distance from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetTabDistance(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                  const unsigned short cParams,
                                                                  _Out_ SHORT* const psDistance) const
{
    bool fSuccess = false;
    *psDistance = s_sDefaultTabDistance;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *psDistance = rgusParams[0];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 1.
    if (*psDistance == 0)
    {
        *psDistance = s_sDefaultTabDistance;
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves the type of tab clearing operation from the parameter pool stored during Param actions.
// Arguments:
// - psClearType - Memory location to receive the clear type
// Return Value:
// - True if we successfully pulled the tab clear type from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetTabClearType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                   const unsigned short cParams,
                                                                   _Out_ SHORT* const psClearType) const
{
    bool fSuccess = false;
    *psClearType = s_sDefaultTabClearType;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *psClearType = rgusParams[0];
        fSuccess = true;
    }
    return fSuccess;
}

// Routine Description:
// - Retrieves a designate charset type from the intermediate we've stored. False otherwise.
// Arguments:
// - pDesignateType - Memory location to receive the designate type.
// Return Value:
// - True if we successfully pulled the designate type from the intermediate we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetDesignateType(const wchar_t wchIntermediate,
                                                                    _Out_ DesignateCharsetTypes* const pDesignateType) const
{
    bool fSuccess = false;
    *pDesignateType = s_DefaultDesignateCharsetType;

    switch (wchIntermediate)
    {
    case '(':
        *pDesignateType = DesignateCharsetTypes::G0;
        fSuccess = true;
        break;
    case ')':
    case '-':
        *pDesignateType = DesignateCharsetTypes::G1;
        fSuccess = true;
        break;
    case '*':
    case '.':
        *pDesignateType = DesignateCharsetTypes::G2;
        fSuccess = true;
        break;
    case '+':
    case '/':
        *pDesignateType = DesignateCharsetTypes::G3;
        fSuccess = true;
        break;
    }

    return fSuccess;
}

// Routine Description:
// - Returns true if the engine should dispatch on the last charater of a string
//      always, even if the sequence hasn't normally dispatched.
//   If this is false, the engine will persist its state across calls to
//      ProcessString, and dispatch only at the end of the sequence.
// Return Value:
// - True iff we should manually dispatch on the last character of a string.
bool OutputStateMachineEngine::FlushAtEndOfString() const
{
    return false;
}

// Routine Description:
// - Returns true if the engine should dispatch control characters in the Escape
//      state. Typically, control characters are immediately executed in the
//      Escape state without returning to ground. If this returns true, the
//      state machine will instead call ActionExecuteFromEscape and then enter
//      the Ground state when a control character is encountered in the escape
//      state.
// Return Value:
// - True iff we should return to the Ground state when the state machine
//      encounters a Control (C0) character in the Escape state.
bool OutputStateMachineEngine::DispatchControlCharsFromEscape() const
{
    return false;
}

// Routine Description:
// - Returns false if the engine wants to be able to collect intermediate
//   characters in the Escape state. We do want to buffer characters as
//   intermediates. We need them for things like Designate G0 Character Set
// Return Value:
// - True iff we should dispatch in the Escape state when we encounter a
//   Intermediate character.
bool OutputStateMachineEngine::DispatchIntermediatesFromEscape() const
{
    return false;
}

// Routine Description:
// - Converts a hex character to its equivalent integer value.
// Arguments:
// - wch - Character to convert.
// - puiValue - recieves the int value of the char
// Return Value:
// - true iff the character is a hex character.
bool OutputStateMachineEngine::s_HexToUint(const wchar_t wch,
                                           _Out_ unsigned int* const puiValue)
{
    *puiValue = 0;
    bool fSuccess = false;
    if (wch >= L'0' && wch <= L'9')
    {
        *puiValue = wch - L'0';
        fSuccess = true;
    }
    else if (wch >= L'A' && wch <= L'F')
    {
        *puiValue = (wch - L'A') + 10;
        fSuccess = true;
    }
    else if (wch >= L'a' && wch <= L'f')
    {
        *puiValue = (wch - L'a') + 10;
        fSuccess = true;
    }
    return fSuccess;
}

// Routine Description:
// - Determines if a character is a valid number character, 0-9.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool OutputStateMachineEngine::s_IsNumber(const wchar_t wch)
{
    return wch >= L'0' && wch <= L'9'; // 0x30 - 0x39
}

// Routine Description:
// - Determines if a character is a valid hex character, 0-9a-fA-F.
// Arguments:
// - wch - Character to check.
// Return Value:
// - True if it is. False if it isn't.
bool OutputStateMachineEngine::s_IsHexNumber(const wchar_t wch)
{
    return (wch >= L'0' && wch <= L'9') || // 0x30 - 0x39
           (wch >= L'A' && wch <= L'F') ||
           (wch >= L'a' && wch <= L'f');
}

// Routine Description:
// - Given a color spec string, attempts to parse the color that's encoded.
//   The only supported spec currently is the following:
//      spec: a color in the following format:
//          "rgb:<red>/<green>/<blue>"
//          where <color> is one or two hex digits, upper or lower case.
// Arguments:
// - pwchBuffer - The string containing the color spec string to parse.
// - cchBuffer - a the length of the pwchBuffer
// - pRgb - recieves the color that we parsed
// Return Value:
// - True if a color was successfully parsed
bool OutputStateMachineEngine::s_ParseColorSpec(_In_reads_(cchBuffer) const wchar_t* const pwchBuffer,
                                                const size_t cchBuffer,
                                                _Out_ DWORD* const pRgb)
{
    const wchar_t* pwchCurr = pwchBuffer;
    const wchar_t* const pwchEnd = pwchBuffer + cchBuffer;
    bool foundRGB = false;
    bool foundValidColorSpec = false;
    unsigned int rguiColorValues[3] = { 0 };
    bool fSuccess = false;
    // We can have anywhere between [11,15] characters
    // 9 "rgb:h/h/h"
    // 12 "rgb:hh/hh/hh"
    // Any fewer cannot be valid, and any more will be too many.
    // Return early in this case.
    //      We'll still have to bounds check when parsing the hh/hh/hh values
    if (cchBuffer < 9 || cchBuffer > 12)
    {
        return false;
    }

    // Now we look for "rgb:"
    // Other colorspaces are theoretically possible, but we don't support them.

    if ((pwchCurr[0] == L'r') &&
        (pwchCurr[1] == L'g') &&
        (pwchCurr[2] == L'b') &&
        (pwchCurr[3] == L':'))
    {
        foundRGB = true;
    }
    pwchCurr += 4;

    if (foundRGB)
    {
        // Colorspecs are up to hh/hh/hh, for 1-2 h's
        for (size_t component = 0; component < 3; component++)
        {
            bool foundColor = false;
            unsigned int* const pValue = &(rguiColorValues[component]);
            for (size_t i = 0; i < 3; i++)
            {
                const wchar_t wch = *pwchCurr;
                pwchCurr++;

                if (s_IsHexNumber(wch))
                {
                    *pValue *= 16;
                    unsigned int intVal = 0;
                    if (s_HexToUint(wch, &intVal))
                    {
                        *pValue += intVal;
                    }
                    else
                    {
                        // Encountered something weird oh no
                        foundColor = false;
                        break;
                    }
                    // If we're on the blue component, we're not going to see a /.
                    // Break out once we hit the end.
                    if (component == 2 && pwchCurr == pwchEnd)
                    {
                        foundValidColorSpec = true;
                        break;
                    }
                }
                else if (wch == L'/')
                {
                    // Break this component, and start the next one.
                    foundColor = true;
                    break;
                }
                else
                {
                    // Encountered something weird oh no
                    foundColor = false;
                    break;
                }
            }
            if (!foundColor || pwchCurr == pwchEnd)
            {
                // Indicates there was a some error parsing color
                //  or we're at the end of the string.
                break;
            }
        }
    }
    // Only if we find a valid colorspec can we pass it out successfully.
    if (foundValidColorSpec)
    {
        DWORD color = RGB(LOBYTE(rguiColorValues[0]),
                          LOBYTE(rguiColorValues[1]),
                          LOBYTE(rguiColorValues[2]));

        *pRgb = color;
        fSuccess = true;
    }
    return fSuccess;
}

// Routine Description:
// - OSC 4 ; c ; spec ST
//      c: the index of the ansi color table
//      spec: a color in the following format:
//          "rgb:<red>/<green>/<blue>"
//          where <color> is two hex digits
// Arguments:
// - pwchOscStringBuffer - a pointer to the Osc String to parse
// - cchOscString - the length of the Osc String
// - pTableIndex - a pointer that recieves the table index
// - pRgb - a pointer that recieves the color that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if a table index and color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColorTable(_In_reads_(cchOscString) const wchar_t* const pwchOscStringBuffer,
                                                    const size_t cchOscString,
                                                    _Out_ size_t* const pTableIndex,
                                                    _Out_ DWORD* const pRgb) const
{
    *pTableIndex = 0;
    *pRgb = 0;
    const wchar_t* pwchCurr = pwchOscStringBuffer;
    const wchar_t* const pwchEnd = pwchOscStringBuffer + cchOscString;
    size_t _TableIndex = 0;

    bool foundTableIndex = false;
    bool fSuccess = false;
    // We can have anywhere between [11,16] characters
    // 11 "#;rgb:h/h/h"
    // 16 "###;rgb:hh/hh/hh"
    // Any fewer cannot be valid, and any more will be too many.
    // Return early in this case.
    //      We'll still have to bounds check when parsing the hh/hh/hh values
    if (cchOscString < 11 || cchOscString > 16)
    {
        return false;
    }

    // First try to get the table index, a number between [0,256]
    for (size_t i = 0; i < 4; i++)
    {
        const wchar_t wch = *pwchCurr;
        if (s_IsNumber(wch))
        {
            _TableIndex *= 10;
            _TableIndex += wch - L'0';

            pwchCurr++;
        }
        else if (wch == L';' && i > 0)
        {
            // We need to explicitly pass in a number, we can't default to 0 if
            //  there's no param
            pwchCurr++;
            foundTableIndex = true;
            break;
        }
        else
        {
            // Found an unexpected character, fail.
            break;
        }
    }
    // Now we look for "rgb:"
    // Other colorspaces are theoretically possible, but we don't support them.
    if (foundTableIndex)
    {
        DWORD color = 0;
        fSuccess = s_ParseColorSpec(pwchCurr, pwchEnd - pwchCurr, &color);

        if (fSuccess)
        {
            *pTableIndex = _TableIndex;
            *pRgb = color;
        }
    }

    return fSuccess;
}

// Routine Description:
// - OSC 10, 11, 12 ; spec ST
//      spec: a color in the following format:
//          "rgb:<red>/<green>/<blue>"
//          where <color> is two hex digits
// Arguments:
// - pwchOscStringBuffer - a pointer to the Osc String to parse
// - cchOscString - the length of the Osc String
// - pRgb - a pointer that recieves the color that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if a table index and color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColor(_In_reads_(cchOscString) const wchar_t* const pwchOscStringBuffer,
                                               const size_t cchOscString,
                                               _Out_ DWORD* const pRgb) const
{
    *pRgb = 0;
    const wchar_t* pwchCurr = pwchOscStringBuffer;
    const wchar_t* const pwchEnd = pwchOscStringBuffer + cchOscString;

    bool fSuccess = false;

    DWORD color = 0;
    fSuccess = s_ParseColorSpec(pwchCurr, pwchEnd - pwchCurr, &color);

    if (fSuccess)
    {
        *pRgb = color;
    }

    return fSuccess;
}

// Method Description:
// - Retrieves the type of window manipulation operation from the parameter pool
//      stored during Param actions.
//  This is kept seperate from the input version, as there may be
//      codes that are supported in one direction but not the other.
// Arguments:
// - rgusParams - Array of parameters collected
// - cParams - Number of parameters we've collected
// - puiFunction - Memory location to receive the function type
// Return Value:
// - True iff we successfully pulled the function type from the parameters
bool OutputStateMachineEngine::_GetWindowManipulationType(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                          const unsigned short cParams,
                                                          _Out_ unsigned int* const puiFunction) const
{
    bool fSuccess = false;
    *puiFunction = s_DefaultWindowManipulationType;

    if (cParams > 0)
    {
        switch (rgusParams[0])
        {
        case DispatchTypes::WindowManipulationType::RefreshWindow:
            *puiFunction = DispatchTypes::WindowManipulationType::RefreshWindow;
            fSuccess = true;
            break;
        case DispatchTypes::WindowManipulationType::ResizeWindowInCharacters:
            *puiFunction = DispatchTypes::WindowManipulationType::ResizeWindowInCharacters;
            fSuccess = true;
            break;
        default:
            fSuccess = false;
            break;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Retrieves a distance for a scroll operation from the parameter pool stored during Param actions.
// Arguments:
// - puiDistance - Memory location to receive the distance
// Return Value:
// - True if we successfully pulled the scroll distance from the parameters we've stored. False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetCursorStyle(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                  const unsigned short cParams,
                                                                  _Out_ DispatchTypes::CursorStyle* const pCursorStyle) const
{
    bool fSuccess = false;
    *pCursorStyle = s_defaultCursorStyle;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *pCursorStyle = (DispatchTypes::CursorStyle)rgusParams[0];
        fSuccess = true;
    }

    return fSuccess;
}

// Method Description:
// - Sets us up to have another terminal acting as the tty instead of conhost.
//      We'll set a couple members, and if they aren't null, when we get a
//      sequence we don't understand, we'll pass it along to the terminal
//      instead of eating it ourselves.
// Arguments:
// - pTtyConnection: This is a TerminalOutputConnection that we can write the
//      sequence we didn't understand to.
// - pfnFlushToTerminal: This is a callback to the underlying state machine to
//      trigger it to call ActionPassThroughString with whatever sequence it's
//      currently processing.
// Return Value:
// - <none>
void OutputStateMachineEngine::SetTerminalConnection(ITerminalOutputConnection* const pTtyConnection,
                                                     std::function<bool()> pfnFlushToTerminal)
{
    this->_pTtyConnection = pTtyConnection;
    this->_pfnFlushToTerminal = pfnFlushToTerminal;
}

// Routine Description:
// - Retrieves a number of times to repeat the last graphical character
// Arguments:
// - puiRepeatCount - Memory location to receive the repeat count
// Return Value:
// - True if we successfully pulled the repeat count from the parameters.
//   False otherwise.
_Success_(return ) bool OutputStateMachineEngine::_GetRepeatCount(_In_reads_(cParams) const unsigned short* const rgusParams,
                                                                  const unsigned short cParams,
                                                                  _Out_ unsigned int* const puiRepeatCount) const noexcept
{
    bool fSuccess = false;
    *puiRepeatCount = s_uiDefaultRepeatCount;

    if (cParams == 0)
    {
        // Empty parameter sequences should use the default
        fSuccess = true;
    }
    else if (cParams == 1)
    {
        // If there's one parameter, use it.
        *puiRepeatCount = rgusParams[0];
        fSuccess = true;
    }

    // Distances of 0 should be changed to 1.
    if (*puiRepeatCount == 0)
    {
        *puiRepeatCount = s_uiDefaultRepeatCount;
    }

    return fSuccess;
}

// Method Description:
// - Clears our last stored character. The last stored character is the last
//      graphical character we printed, which is reset if any other action is
//      dispatched.
// Arguments:
// - <none>
// Return Value:
// - <none>
void OutputStateMachineEngine::_ClearLastChar() noexcept
{
    _lastPrintedChar = AsciiChars::NUL;
}
