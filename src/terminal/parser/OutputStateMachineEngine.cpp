// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"
#include "OutputStateMachineEngine.hpp"
#include "base64.hpp"

#include "ascii.hpp"
#include "../../types/inc/utils.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;

// the console uses 0xffffffff as an "invalid color" value
constexpr COLORREF INVALID_COLOR = 0xffffffff;

// takes ownership of pDispatch
OutputStateMachineEngine::OutputStateMachineEngine(std::unique_ptr<ITermDispatch> pDispatch) :
    _dispatch(std::move(pDispatch)),
    _pfnFlushToTerminal(nullptr),
    _pTtyConnection(nullptr),
    _lastPrintedChar(AsciiChars::NUL)
{
    THROW_HR_IF_NULL(E_INVALIDARG, _dispatch.get());
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
    switch (wch)
    {
    case AsciiChars::NUL:
        // microsoft/terminal#1825 - VT applications expect to be able to write NUL
        // and have _nothing_ happen. Filter the NULs here, so they don't fill the
        // buffer with empty spaces.
        break;
    case AsciiChars::BEL:
        _dispatch->WarningBell();
        // microsoft/terminal#2952
        // If we're attached to a terminal, let's also pass the BEL through.
        if (_pfnFlushToTerminal != nullptr)
        {
            _pfnFlushToTerminal();
        }
        break;
    case AsciiChars::BS:
        _dispatch->CursorBackward(1);
        break;
    case AsciiChars::TAB:
        _dispatch->ForwardTab(1);
        break;
    case AsciiChars::CR:
        _dispatch->CarriageReturn();
        break;
    case AsciiChars::LF:
    case AsciiChars::FF:
    case AsciiChars::VT:
        // LF, FF, and VT are identical in function.
        _dispatch->LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
        break;
    case AsciiChars::SI:
        _dispatch->LockingShift(0);
        break;
    case AsciiChars::SO:
        _dispatch->LockingShift(1);
        break;
    default:
        _dispatch->Print(wch);
        break;
    }

    _ClearLastChar();

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
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPrintString(const std::wstring_view string)
{
    if (string.empty())
    {
        return true;
    }

    // Stash the last character of the string, if it's a graphical character
    const wchar_t wch = string.back();
    if (wch >= AsciiChars::SPC)
    {
        _lastPrintedChar = wch;
    }

    _dispatch->PrintString(string); // call print

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
// - string - string to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPassThroughString(const std::wstring_view string)
{
    bool success = true;
    if (_pTtyConnection != nullptr)
    {
        const auto hr = _pTtyConnection->WriteTerminalW(string);
        LOG_IF_FAILED(hr);
        success = SUCCEEDED(hr);
    }
    // If there's not a TTY connection, our previous behavior was to eat the string.

    return success;
}

// Routine Description:
// - Triggers the EscDispatch action to indicate that the listener should handle
//      a simple escape sequence. These sequences traditionally start with ESC
//      and a simple letter. No complicated parameters.
// Arguments:
// - id - Identifier of the escape sequence to dispatch.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionEscDispatch(const VTID id)
{
    bool success = false;

    switch (id)
    {
    case EscActionCodes::ST_StringTerminator:
        // This is the 7-bit string terminator, which is essentially a no-op.
        success = true;
        break;
    case EscActionCodes::DECSC_CursorSave:
        success = _dispatch->CursorSaveState();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSC);
        break;
    case EscActionCodes::DECRC_CursorRestore:
        success = _dispatch->CursorRestoreState();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECRC);
        break;
    case EscActionCodes::DECKPAM_KeypadApplicationMode:
        success = _dispatch->SetKeypadMode(true);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECKPAM);
        break;
    case EscActionCodes::DECKPNM_KeypadNumericMode:
        success = _dispatch->SetKeypadMode(false);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECKPNM);
        break;
    case EscActionCodes::NEL_NextLine:
        success = _dispatch->LineFeed(DispatchTypes::LineFeedType::WithReturn);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::NEL);
        break;
    case EscActionCodes::IND_Index:
        success = _dispatch->LineFeed(DispatchTypes::LineFeedType::WithoutReturn);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::IND);
        break;
    case EscActionCodes::RI_ReverseLineFeed:
        success = _dispatch->ReverseLineFeed();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::RI);
        break;
    case EscActionCodes::HTS_HorizontalTabSet:
        success = _dispatch->HorizontalTabSet();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::HTS);
        break;
    case EscActionCodes::DECID_IdentifyDevice:
        success = _dispatch->DeviceAttributes();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DA);
        break;
    case EscActionCodes::RIS_ResetToInitialState:
        success = _dispatch->HardReset();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::RIS);
        break;
    case EscActionCodes::SS2_SingleShift:
        success = _dispatch->SingleShift(2);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::SS2);
        break;
    case EscActionCodes::SS3_SingleShift:
        success = _dispatch->SingleShift(3);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::SS3);
        break;
    case EscActionCodes::LS2_LockingShift:
        success = _dispatch->LockingShift(2);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::LS2);
        break;
    case EscActionCodes::LS3_LockingShift:
        success = _dispatch->LockingShift(3);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::LS3);
        break;
    case EscActionCodes::LS1R_LockingShift:
        success = _dispatch->LockingShiftRight(1);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::LS1R);
        break;
    case EscActionCodes::LS2R_LockingShift:
        success = _dispatch->LockingShiftRight(2);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::LS2R);
        break;
    case EscActionCodes::LS3R_LockingShift:
        success = _dispatch->LockingShiftRight(3);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::LS3R);
        break;
    case EscActionCodes::DECDHL_DoubleHeightLineTop:
        _dispatch->SetLineRendition(LineRendition::DoubleHeightTop);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECDHL);
        break;
    case EscActionCodes::DECDHL_DoubleHeightLineBottom:
        _dispatch->SetLineRendition(LineRendition::DoubleHeightBottom);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECDHL);
        break;
    case EscActionCodes::DECSWL_SingleWidthLine:
        _dispatch->SetLineRendition(LineRendition::SingleWidth);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSWL);
        break;
    case EscActionCodes::DECDWL_DoubleWidthLine:
        _dispatch->SetLineRendition(LineRendition::DoubleWidth);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECDWL);
        break;
    case EscActionCodes::DECALN_ScreenAlignmentPattern:
        success = _dispatch->ScreenAlignmentPattern();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECALN);
        break;
    default:
        const auto commandChar = id[0];
        const auto commandParameter = id.SubSequence(1);
        switch (commandChar)
        {
        case '%':
            success = _dispatch->DesignateCodingSystem(commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DOCS);
            break;
        case '(':
            success = _dispatch->Designate94Charset(0, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG0);
            break;
        case ')':
            success = _dispatch->Designate94Charset(1, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG1);
            break;
        case '*':
            success = _dispatch->Designate94Charset(2, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG2);
            break;
        case '+':
            success = _dispatch->Designate94Charset(3, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG3);
            break;
        case '-':
            success = _dispatch->Designate96Charset(1, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG1);
            break;
        case '.':
            success = _dispatch->Designate96Charset(2, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG2);
            break;
        case '/':
            success = _dispatch->Designate96Charset(3, commandParameter);
            TermTelemetry::Instance().Log(TermTelemetry::Codes::DesignateG3);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            success = false;
            break;
        }
    }

    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !success)
    {
        success = _pfnFlushToTerminal();
    }

    _ClearLastChar();

    return success;
}

// Method Description:
// - Triggers the Vt52EscDispatch action to indicate that the listener should handle
//      a VT52 escape sequence. These sequences start with ESC and a single letter,
//      sometimes followed by parameters.
// Arguments:
// - id - Identifier of the VT52 sequence to dispatch.
// - parameters - Set of parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionVt52EscDispatch(const VTID id, const VTParameters parameters)
{
    bool success = false;

    switch (id)
    {
    case Vt52ActionCodes::CursorUp:
        success = _dispatch->CursorUp(1);
        break;
    case Vt52ActionCodes::CursorDown:
        success = _dispatch->CursorDown(1);
        break;
    case Vt52ActionCodes::CursorRight:
        success = _dispatch->CursorForward(1);
        break;
    case Vt52ActionCodes::CursorLeft:
        success = _dispatch->CursorBackward(1);
        break;
    case Vt52ActionCodes::EnterGraphicsMode:
        success = _dispatch->Designate94Charset(0, DispatchTypes::CharacterSets::DecSpecialGraphics);
        break;
    case Vt52ActionCodes::ExitGraphicsMode:
        success = _dispatch->Designate94Charset(0, DispatchTypes::CharacterSets::ASCII);
        break;
    case Vt52ActionCodes::CursorToHome:
        success = _dispatch->CursorPosition(1, 1);
        break;
    case Vt52ActionCodes::ReverseLineFeed:
        success = _dispatch->ReverseLineFeed();
        break;
    case Vt52ActionCodes::EraseToEndOfScreen:
        success = _dispatch->EraseInDisplay(DispatchTypes::EraseType::ToEnd);
        break;
    case Vt52ActionCodes::EraseToEndOfLine:
        success = _dispatch->EraseInLine(DispatchTypes::EraseType::ToEnd);
        break;
    case Vt52ActionCodes::DirectCursorAddress:
        // VT52 cursor addresses are provided as ASCII characters, with
        // the lowest value being a space, representing an address of 1.
        success = _dispatch->CursorPosition(parameters.at(0).value() - ' ' + 1, parameters.at(1).value() - ' ' + 1);
        break;
    case Vt52ActionCodes::Identify:
        success = _dispatch->Vt52DeviceAttributes();
        break;
    case Vt52ActionCodes::EnterAlternateKeypadMode:
        success = _dispatch->SetKeypadMode(true);
        break;
    case Vt52ActionCodes::ExitAlternateKeypadMode:
        success = _dispatch->SetKeypadMode(false);
        break;
    case Vt52ActionCodes::ExitVt52Mode:
        success = _dispatch->SetMode(DispatchTypes::ModeParams::DECANM_AnsiMode);
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        success = false;
        break;
    }

    _ClearLastChar();

    return success;
}

// Routine Description:
// - Triggers the CsiDispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)
{
    bool success = false;

    switch (id)
    {
    case CsiActionCodes::CUU_CursorUp:
        success = _dispatch->CursorUp(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CUU);
        break;
    case CsiActionCodes::CUD_CursorDown:
        success = _dispatch->CursorDown(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CUD);
        break;
    case CsiActionCodes::CUF_CursorForward:
        success = _dispatch->CursorForward(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CUF);
        break;
    case CsiActionCodes::CUB_CursorBackward:
        success = _dispatch->CursorBackward(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CUB);
        break;
    case CsiActionCodes::CNL_CursorNextLine:
        success = _dispatch->CursorNextLine(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CNL);
        break;
    case CsiActionCodes::CPL_CursorPrevLine:
        success = _dispatch->CursorPrevLine(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CPL);
        break;
    case CsiActionCodes::CHA_CursorHorizontalAbsolute:
    case CsiActionCodes::HPA_HorizontalPositionAbsolute:
        success = _dispatch->CursorHorizontalPositionAbsolute(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CHA);
        break;
    case CsiActionCodes::VPA_VerticalLinePositionAbsolute:
        success = _dispatch->VerticalLinePositionAbsolute(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::VPA);
        break;
    case CsiActionCodes::HPR_HorizontalPositionRelative:
        success = _dispatch->HorizontalPositionRelative(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::HPR);
        break;
    case CsiActionCodes::VPR_VerticalPositionRelative:
        success = _dispatch->VerticalPositionRelative(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::VPR);
        break;
    case CsiActionCodes::CUP_CursorPosition:
    case CsiActionCodes::HVP_HorizontalVerticalPosition:
        success = _dispatch->CursorPosition(parameters.at(0), parameters.at(1));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CUP);
        break;
    case CsiActionCodes::DECSTBM_SetScrollingRegion:
        success = _dispatch->SetTopBottomScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSTBM);
        break;
    case CsiActionCodes::ICH_InsertCharacter:
        success = _dispatch->InsertCharacter(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::ICH);
        break;
    case CsiActionCodes::DCH_DeleteCharacter:
        success = _dispatch->DeleteCharacter(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DCH);
        break;
    case CsiActionCodes::ED_EraseDisplay:
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch->EraseInDisplay(eraseType);
        });
        TermTelemetry::Instance().Log(TermTelemetry::Codes::ED);
        break;
    case CsiActionCodes::EL_EraseLine:
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch->EraseInLine(eraseType);
        });
        TermTelemetry::Instance().Log(TermTelemetry::Codes::EL);
        break;
    case CsiActionCodes::DECSET_PrivateModeSet:
        success = parameters.for_each([&](const auto mode) {
            return _dispatch->SetMode(DispatchTypes::DECPrivateMode(mode));
        });
        //TODO: MSFT:6367459 Add specific logging for each of the DECSET/DECRST codes
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSET);
        break;
    case CsiActionCodes::DECRST_PrivateModeReset:
        success = parameters.for_each([&](const auto mode) {
            return _dispatch->ResetMode(DispatchTypes::DECPrivateMode(mode));
        });
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECRST);
        break;
    case CsiActionCodes::SGR_SetGraphicsRendition:
        success = _dispatch->SetGraphicsRendition(parameters);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::SGR);
        break;
    case CsiActionCodes::DSR_DeviceStatusReport:
        success = _dispatch->DeviceStatusReport(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DSR);
        break;
    case CsiActionCodes::DA_DeviceAttributes:
        success = parameters.at(0).value_or(0) == 0 && _dispatch->DeviceAttributes();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DA);
        break;
    case CsiActionCodes::DA2_SecondaryDeviceAttributes:
        success = parameters.at(0).value_or(0) == 0 && _dispatch->SecondaryDeviceAttributes();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DA2);
        break;
    case CsiActionCodes::DA3_TertiaryDeviceAttributes:
        success = parameters.at(0).value_or(0) == 0 && _dispatch->TertiaryDeviceAttributes();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DA3);
        break;
    case CsiActionCodes::DECREQTPARM_RequestTerminalParameters:
        success = _dispatch->RequestTerminalParameters(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECREQTPARM);
        break;
    case CsiActionCodes::SU_ScrollUp:
        success = _dispatch->ScrollUp(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::SU);
        break;
    case CsiActionCodes::SD_ScrollDown:
        success = _dispatch->ScrollDown(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::SD);
        break;
    case CsiActionCodes::ANSISYSSC_CursorSave:
        success = parameters.empty() && _dispatch->CursorSaveState();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::ANSISYSSC);
        break;
    case CsiActionCodes::ANSISYSRC_CursorRestore:
        success = parameters.empty() && _dispatch->CursorRestoreState();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::ANSISYSRC);
        break;
    case CsiActionCodes::IL_InsertLine:
        success = _dispatch->InsertLine(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::IL);
        break;
    case CsiActionCodes::DL_DeleteLine:
        success = _dispatch->DeleteLine(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DL);
        break;
    case CsiActionCodes::CHT_CursorForwardTab:
        success = _dispatch->ForwardTab(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CHT);
        break;
    case CsiActionCodes::CBT_CursorBackTab:
        success = _dispatch->BackwardsTab(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::CBT);
        break;
    case CsiActionCodes::TBC_TabClear:
        success = parameters.for_each([&](const auto clearType) {
            return _dispatch->TabClear(clearType);
        });
        TermTelemetry::Instance().Log(TermTelemetry::Codes::TBC);
        break;
    case CsiActionCodes::ECH_EraseCharacters:
        success = _dispatch->EraseCharacters(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::ECH);
        break;
    case CsiActionCodes::DTTERM_WindowManipulation:
        success = _dispatch->WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DTTERM_WM);
        break;
    case CsiActionCodes::REP_RepeatCharacter:
        // Handled w/o the dispatch. This function is unique in that way
        // If this were in the ITerminalDispatch, then each
        // implementation would effectively be the same, calling only
        // functions that are already part of the interface.
        // Print the last graphical character a number of times.
        if (_lastPrintedChar != AsciiChars::NUL)
        {
            const size_t repeatCount = parameters.at(0);
            std::wstring wstr(repeatCount, _lastPrintedChar);
            _dispatch->PrintString(wstr);
        }
        success = true;
        TermTelemetry::Instance().Log(TermTelemetry::Codes::REP);
        break;
    case CsiActionCodes::DECSCUSR_SetCursorStyle:
        success = _dispatch->SetCursorStyle(parameters.at(0));
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSCUSR);
        break;
    case CsiActionCodes::DECSTR_SoftReset:
        success = _dispatch->SoftReset();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::DECSTR);
        break;

    case CsiActionCodes::XT_PushSgr:
    case CsiActionCodes::XT_PushSgrAlias:
        success = _dispatch->PushGraphicsRendition(parameters);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::XTPUSHSGR);
        break;

    case CsiActionCodes::XT_PopSgr:
    case CsiActionCodes::XT_PopSgrAlias:
        success = _dispatch->PopGraphicsRendition();
        TermTelemetry::Instance().Log(TermTelemetry::Codes::XTPOPSGR);
        break;

    default:
        // If no functions to call, overall dispatch was a failure.
        success = false;
        break;
    }

    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !success)
    {
        success = _pfnFlushToTerminal();
    }

    _ClearLastChar();

    return success;
}

// Routine Description:
// - Triggers the DcsDispatch action to indicate that the listener should handle
//      a control sequence. Returns the handler function that is to be used to
//      process the subsequent data string characters in the sequence.
// Arguments:
// - id - Identifier of the control sequence to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - the data string handler function or nullptr if the sequence is not supported
IStateMachineEngine::StringHandler OutputStateMachineEngine::ActionDcsDispatch(const VTID /*id*/, const VTParameters /*parameters*/) noexcept
{
    StringHandler handler = nullptr;

    _ClearLastChar();

    return handler;
}

// Routine Description:
// - Triggers the Clear action to indicate that the state machine should erase
//      all internal state.
// Arguments:
// - <none>
// Return Value:
// - <none>
bool OutputStateMachineEngine::ActionClear() noexcept
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
bool OutputStateMachineEngine::ActionIgnore() noexcept
{
    // do nothing.
    return true;
}

// Routine Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - wch - Character to dispatch. This will be a BEL or ST char.
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dispatch.
bool OutputStateMachineEngine::ActionOscDispatch(const wchar_t /*wch*/,
                                                 const size_t parameter,
                                                 const std::wstring_view string)
{
    bool success = false;

    switch (parameter)
    {
    case OscActionCodes::SetIconAndWindowTitle:
    case OscActionCodes::SetWindowIcon:
    case OscActionCodes::SetWindowTitle:
    {
        std::wstring title;
        success = _GetOscTitle(string, title);
        success = success && _dispatch->SetWindowTitle(title);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCWT);
        break;
    }
    case OscActionCodes::SetColor:
    {
        std::vector<size_t> tableIndexes;
        std::vector<DWORD> colors;
        success = _GetOscSetColorTable(string, tableIndexes, colors);
        for (size_t i = 0; i < tableIndexes.size(); i++)
        {
            const auto tableIndex = til::at(tableIndexes, i);
            const auto rgb = til::at(colors, i);
            success = success && _dispatch->SetColorTableEntry(tableIndex, rgb);
        }
        TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCCT);
        break;
    }
    case OscActionCodes::SetForegroundColor:
    case OscActionCodes::SetBackgroundColor:
    case OscActionCodes::SetCursorColor:
    {
        std::vector<DWORD> colors;
        success = _GetOscSetColor(string, colors);
        if (success)
        {
            size_t commandIndex = parameter;
            size_t colorIndex = 0;

            if (commandIndex == OscActionCodes::SetForegroundColor && colors.size() > colorIndex)
            {
                const auto color = til::at(colors, colorIndex);
                if (color != INVALID_COLOR)
                {
                    success = success && _dispatch->SetDefaultForeground(color);
                }
                TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCFG);
                commandIndex++;
                colorIndex++;
            }

            if (commandIndex == OscActionCodes::SetBackgroundColor && colors.size() > colorIndex)
            {
                const auto color = til::at(colors, colorIndex);
                if (color != INVALID_COLOR)
                {
                    success = success && _dispatch->SetDefaultBackground(color);
                }
                TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCBG);
                commandIndex++;
                colorIndex++;
            }

            if (commandIndex == OscActionCodes::SetCursorColor && colors.size() > colorIndex)
            {
                const auto color = til::at(colors, colorIndex);
                if (color != INVALID_COLOR)
                {
                    success = success && _dispatch->SetCursorColor(color);
                }
                TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCSCC);
                commandIndex++;
                colorIndex++;
            }
        }
        break;
    }
    case OscActionCodes::SetClipboard:
    {
        std::wstring setClipboardContent;
        bool queryClipboard = false;
        success = _GetOscSetClipboard(string, setClipboardContent, queryClipboard);
        if (success && !queryClipboard)
        {
            success = _dispatch->SetClipboard(setClipboardContent);
        }
        TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCSCB);
        break;
    }
    case OscActionCodes::ResetCursorColor:
    {
        success = _dispatch->SetCursorColor(INVALID_COLOR);
        TermTelemetry::Instance().Log(TermTelemetry::Codes::OSCRCC);
        break;
    }
    case OscActionCodes::Hyperlink:
    {
        std::wstring params;
        std::wstring uri;
        success = _ParseHyperlink(string, params, uri);
        if (uri.empty())
        {
            success = success && _dispatch->EndHyperlink();
        }
        else
        {
            success = success && _dispatch->AddHyperlink(uri, params);
        }
        break;
    }
    case OscActionCodes::ConEmuAction:
    {
        success = _dispatch->DoConEmuAction(string);
        break;
    }
    default:
        // If no functions to call, overall dispatch was a failure.
        success = false;
        break;
    }

    // If we were unable to process the string, and there's a TTY attached to us,
    //      trigger the state machine to flush the string to the terminal.
    if (_pfnFlushToTerminal != nullptr && !success)
    {
        success = _pfnFlushToTerminal();
    }

    _ClearLastChar();

    return success;
}

// Routine Description:
// - Triggers the Ss3Dispatch action to indicate that the listener should handle
//      a control sequence. These sequences perform various API-type commands
//      that can include many parameters.
// Arguments:
// - wch - Character to dispatch.
// - parameters - set of numeric parameters collected while parsing the sequence.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionSs3Dispatch(const wchar_t /*wch*/, const VTParameters /*parameters*/) noexcept
{
    // The output engine doesn't handle any SS3 sequences.
    _ClearLastChar();
    return false;
}

// Routine Description:
// - Null terminates, then returns, the string that we've collected as part of the OSC string.
// Arguments:
// - string - Osc String input
// - title - Where to place the Osc String to use as a title.
// Return Value:
// - True if there was a title to output. (a title with length=0 is still valid)
bool OutputStateMachineEngine::_GetOscTitle(const std::wstring_view string,
                                            std::wstring& title) const
{
    title = string;

    return !string.empty();
}

// Method Description:
// - Returns true if the engine should attempt to parse a control sequence
//      following an SS3 escape prefix.
//   If this is false, an SS3 escape sequence should be dispatched as soon
//      as it is encountered.
// Return Value:
// - True iff we should parse a control sequence following an SS3.
bool OutputStateMachineEngine::ParseControlSequenceAfterSs3() const noexcept
{
    return false;
}

// Routine Description:
// - Returns true if the engine should dispatch on the last character of a string
//      always, even if the sequence hasn't normally dispatched.
//   If this is false, the engine will persist its state across calls to
//      ProcessString, and dispatch only at the end of the sequence.
// Return Value:
// - True iff we should manually dispatch on the last character of a string.
bool OutputStateMachineEngine::FlushAtEndOfString() const noexcept
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
bool OutputStateMachineEngine::DispatchControlCharsFromEscape() const noexcept
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
bool OutputStateMachineEngine::DispatchIntermediatesFromEscape() const noexcept
{
    return false;
}

// Routine Description:
// - OSC 4 ; c ; spec ST
//      c: the index of the ansi color table
//      spec: The colors are specified by name or RGB specification as per XParseColor
//
//   It's possible to have multiple "c ; spec" pairs, which will set the index "c" of the color table
//   with color parsed from "spec" for each pair respectively.
// Arguments:
// - string - the Osc String to parse
// - tableIndexes - receives the table indexes
// - rgbs - receives the colors that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if at least one table index and color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColorTable(const std::wstring_view string,
                                                    std::vector<size_t>& tableIndexes,
                                                    std::vector<DWORD>& rgbs) const noexcept
try
{
    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 2)
    {
        return false;
    }

    std::vector<size_t> newTableIndexes;
    std::vector<DWORD> newRgbs;

    for (size_t i = 0, j = 1; j < parts.size(); i += 2, j += 2)
    {
        unsigned int tableIndex = 0;
        const bool indexSuccess = Utils::StringToUint(til::at(parts, i), tableIndex);
        const auto colorOptional = Utils::ColorFromXTermColor(til::at(parts, j));
        if (indexSuccess && colorOptional.has_value())
        {
            newTableIndexes.push_back(tableIndex);
            newRgbs.push_back(colorOptional.value());
        }
    }

    tableIndexes.swap(newTableIndexes);
    rgbs.swap(newRgbs);

    return tableIndexes.size() > 0 && rgbs.size() > 0;
}
CATCH_LOG_RETURN_FALSE()

#pragma warning(push)
#pragma warning(disable : 26445) // Suppress lifetime check for a reference to gsl::span or std::string_view

// Routine Description:
// - Given a hyperlink string, attempts to parse the URI encoded. An 'id' parameter
//   may be provided.
//   If there is a URI, the well formatted string looks like:
//          "<params>;<URI>"
//   To be specific, params is an optional list of key=value assignments, separated by the ':'. Example:
//          "id=xyz123:foo=bar:baz=value"
//   If there is no URI, we need to close the hyperlink and the string looks like:
//          ";"
// Arguments:
// - string - the string containing the parameters and URI
// - params - where to store the parameters
// - uri - where to store the uri
// Return Value:
// - True if a URI was successfully parsed or if we are meant to close a hyperlink
bool OutputStateMachineEngine::_ParseHyperlink(const std::wstring_view string,
                                               std::wstring& params,
                                               std::wstring& uri) const
{
    params.clear();
    uri.clear();

    if (string == L";")
    {
        return true;
    }

    const size_t midPos = string.find(';');
    if (midPos != std::wstring::npos)
    {
        uri = string.substr(midPos + 1);
        const auto paramStr = string.substr(0, midPos);
        const auto paramParts = Utils::SplitString(paramStr, ':');
        for (const auto& part : paramParts)
        {
            const auto idPos = part.find(hyperlinkIDParameter);
            if (idPos != std::wstring::npos)
            {
                params = part.substr(idPos + hyperlinkIDParameter.size());
            }
        }
        return true;
    }
    return false;
}

#pragma warning(pop)

// Routine Description:
// - OSC 10, 11, 12 ; spec ST
//      spec: The colors are specified by name or RGB specification as per XParseColor
//
//   It's possible to have multiple "spec", which by design equals to a series of OSC command
//   with accumulated Ps. For example "OSC 10;color1;color2" is effectively an "OSC 10;color1"
//   and an "OSC 11;color2".
//
// Arguments:
// - string - the Osc String to parse
// - rgbs - receives the colors that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if at least one color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColor(const std::wstring_view string,
                                               std::vector<DWORD>& rgbs) const noexcept
try
{
    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 1)
    {
        return false;
    }

    std::vector<DWORD> newRgbs;
    for (size_t i = 0; i < parts.size(); i++)
    {
        const auto colorOptional = Utils::ColorFromXTermColor(til::at(parts, i));
        if (colorOptional.has_value())
        {
            newRgbs.push_back(colorOptional.value());
        }
        else
        {
            newRgbs.push_back(INVALID_COLOR);
        }
    }

    rgbs.swap(newRgbs);

    return rgbs.size() > 0;
}
CATCH_LOG_RETURN_FALSE()

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
// - Parse OscSetClipboard parameters with the format `Pc;Pd`. Currently the first parameter `Pc` is
// ignored. The second parameter `Pd` should be a valid base64 string or character `?`.
// Arguments:
// - string - Osc String input.
// - content - Content to set to clipboard.
// - queryClipboard - Whether to get clipboard content and return it to terminal with base64 encoded.
// Return Value:
// - True if there was a valid base64 string or the passed parameter was `?`.
bool OutputStateMachineEngine::_GetOscSetClipboard(const std::wstring_view string,
                                                   std::wstring& content,
                                                   bool& queryClipboard) const noexcept
{
    const size_t pos = string.find(';');
    if (pos != std::wstring_view::npos)
    {
        const std::wstring_view substr = string.substr(pos + 1);
        if (substr == L"?")
        {
            queryClipboard = true;
            return true;
        }
        else
        {
            return Base64::s_Decode(substr, content);
        }
    }

    return false;
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
