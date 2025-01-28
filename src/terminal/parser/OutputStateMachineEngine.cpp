// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "OutputStateMachineEngine.hpp"

#include <conattrs.hpp>

#include "ascii.hpp"
#include "base64.hpp"
#include "stateMachine.hpp"
#include "../../types/inc/utils.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;

constexpr COLORREF COLOR_INQUIRY_COLOR = 0xfeffffff; // It's like INVALID_COLOR but special

// takes ownership of pDispatch
OutputStateMachineEngine::OutputStateMachineEngine(std::unique_ptr<ITermDispatch> pDispatch) :
    _dispatch(std::move(pDispatch)),
    _lastPrintedChar(AsciiChars::NUL)
{
    THROW_HR_IF_NULL(E_INVALIDARG, _dispatch.get());
}

bool OutputStateMachineEngine::EncounteredWin32InputModeSequence() const noexcept
{
    return false;
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
    case AsciiChars::ENQ:
        _dispatch->EnquireAnswerback();
        break;
    case AsciiChars::BEL:
        _dispatch->WarningBell();
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
    case AsciiChars::SUB:
        // The SUB control is used to cancel a control sequence in the same
        // way as CAN, but unlike CAN it also displays an error character,
        // typically a reverse question mark (Unicode substitute form two).
        _dispatch->Print(L'\u2426');
        break;
    case AsciiChars::DEL:
        // The DEL control can sometimes be translated into a printable glyph
        // if a 96-character set is designated, so we need to pass it through
        // to the Print method. If not translated, it will be filtered out
        // there.
        _dispatch->Print(wch);
        break;
    default:
        // GH#1825, GH#10786: VT applications expect to be able to write other
        // control characters and have _nothing_ happen. We filter out these
        // characters here, so they don't fill the buffer.
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
    const auto wch = string.back();
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
// - flush - set to true if the string should be flushed immediately.
// Return Value:
// - true iff we successfully dispatched the sequence.
bool OutputStateMachineEngine::ActionPassThroughString(const std::wstring_view /*string*/) noexcept
{
    return true;
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
    switch (id)
    {
    case EscActionCodes::ST_StringTerminator:
        // This is the 7-bit string terminator, which is essentially a no-op.
        break;
    case EscActionCodes::DECBI_BackIndex:
        _dispatch->BackIndex();
        break;
    case EscActionCodes::DECSC_CursorSave:
        _dispatch->CursorSaveState();
        break;
    case EscActionCodes::DECRC_CursorRestore:
        _dispatch->CursorRestoreState();
        break;
    case EscActionCodes::DECFI_ForwardIndex:
        _dispatch->ForwardIndex();
        break;
    case EscActionCodes::DECKPAM_KeypadApplicationMode:
        _dispatch->SetKeypadMode(true);
        break;
    case EscActionCodes::DECKPNM_KeypadNumericMode:
        _dispatch->SetKeypadMode(false);
        break;
    case EscActionCodes::NEL_NextLine:
        _dispatch->LineFeed(DispatchTypes::LineFeedType::WithReturn);
        break;
    case EscActionCodes::IND_Index:
        _dispatch->LineFeed(DispatchTypes::LineFeedType::WithoutReturn);
        break;
    case EscActionCodes::RI_ReverseLineFeed:
        _dispatch->ReverseLineFeed();
        break;
    case EscActionCodes::HTS_HorizontalTabSet:
        _dispatch->HorizontalTabSet();
        break;
    case EscActionCodes::DECID_IdentifyDevice:
        _dispatch->DeviceAttributes();
        break;
    case EscActionCodes::RIS_ResetToInitialState:
        _dispatch->HardReset();
        break;
    case EscActionCodes::SS2_SingleShift:
        _dispatch->SingleShift(2);
        break;
    case EscActionCodes::SS3_SingleShift:
        _dispatch->SingleShift(3);
        break;
    case EscActionCodes::LS2_LockingShift:
        _dispatch->LockingShift(2);
        break;
    case EscActionCodes::LS3_LockingShift:
        _dispatch->LockingShift(3);
        break;
    case EscActionCodes::LS1R_LockingShift:
        _dispatch->LockingShiftRight(1);
        break;
    case EscActionCodes::LS2R_LockingShift:
        _dispatch->LockingShiftRight(2);
        break;
    case EscActionCodes::LS3R_LockingShift:
        _dispatch->LockingShiftRight(3);
        break;
    case EscActionCodes::DECAC1_AcceptC1Controls:
        _dispatch->AcceptC1Controls(true);
        break;
    case EscActionCodes::S7C1T_Send7bitC1Controls:
        _dispatch->SendC1Controls(false);
        break;
    case EscActionCodes::S8C1T_Send8bitC1Controls:
        _dispatch->SendC1Controls(true);
        break;
    case EscActionCodes::ACS_AnsiLevel1:
        _dispatch->AnnounceCodeStructure(1);
        break;
    case EscActionCodes::ACS_AnsiLevel2:
        _dispatch->AnnounceCodeStructure(2);
        break;
    case EscActionCodes::ACS_AnsiLevel3:
        _dispatch->AnnounceCodeStructure(3);
        break;
    case EscActionCodes::DECDHL_DoubleHeightLineTop:
        _dispatch->SetLineRendition(LineRendition::DoubleHeightTop);
        break;
    case EscActionCodes::DECDHL_DoubleHeightLineBottom:
        _dispatch->SetLineRendition(LineRendition::DoubleHeightBottom);
        break;
    case EscActionCodes::DECSWL_SingleWidthLine:
        _dispatch->SetLineRendition(LineRendition::SingleWidth);
        break;
    case EscActionCodes::DECDWL_DoubleWidthLine:
        _dispatch->SetLineRendition(LineRendition::DoubleWidth);
        break;
    case EscActionCodes::DECALN_ScreenAlignmentPattern:
        _dispatch->ScreenAlignmentPattern();
        break;
    default:
        const auto commandChar = id[0];
        const auto commandParameter = id.SubSequence(1);
        switch (commandChar)
        {
        case '%':
            _dispatch->DesignateCodingSystem(commandParameter);
            break;
        case '(':
            _dispatch->Designate94Charset(0, commandParameter);
            break;
        case ')':
            _dispatch->Designate94Charset(1, commandParameter);
            break;
        case '*':
            _dispatch->Designate94Charset(2, commandParameter);
            break;
        case '+':
            _dispatch->Designate94Charset(3, commandParameter);
            break;
        case '-':
            _dispatch->Designate96Charset(1, commandParameter);
            break;
        case '.':
            _dispatch->Designate96Charset(2, commandParameter);
            break;
        case '/':
            _dispatch->Designate96Charset(3, commandParameter);
            break;
        default:
            break;
        }
    }

    _ClearLastChar();

    return true;
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
    switch (id)
    {
    case Vt52ActionCodes::CursorUp:
        _dispatch->CursorUp(1);
        break;
    case Vt52ActionCodes::CursorDown:
        _dispatch->CursorDown(1);
        break;
    case Vt52ActionCodes::CursorRight:
        _dispatch->CursorForward(1);
        break;
    case Vt52ActionCodes::CursorLeft:
        _dispatch->CursorBackward(1);
        break;
    case Vt52ActionCodes::EnterGraphicsMode:
        _dispatch->Designate94Charset(0, DispatchTypes::CharacterSets::DecSpecialGraphics);
        break;
    case Vt52ActionCodes::ExitGraphicsMode:
        _dispatch->Designate94Charset(0, DispatchTypes::CharacterSets::ASCII);
        break;
    case Vt52ActionCodes::CursorToHome:
        _dispatch->CursorPosition(1, 1);
        break;
    case Vt52ActionCodes::ReverseLineFeed:
        _dispatch->ReverseLineFeed();
        break;
    case Vt52ActionCodes::EraseToEndOfScreen:
        _dispatch->EraseInDisplay(DispatchTypes::EraseType::ToEnd);
        break;
    case Vt52ActionCodes::EraseToEndOfLine:
        _dispatch->EraseInLine(DispatchTypes::EraseType::ToEnd);
        break;
    case Vt52ActionCodes::DirectCursorAddress:
        // VT52 cursor addresses are provided as ASCII characters, with
        // the lowest value being a space, representing an address of 1.
        _dispatch->CursorPosition(parameters.at(0).value() - ' ' + 1, parameters.at(1).value() - ' ' + 1);
        break;
    case Vt52ActionCodes::Identify:
        _dispatch->Vt52DeviceAttributes();
        break;
    case Vt52ActionCodes::EnterAlternateKeypadMode:
        _dispatch->SetKeypadMode(true);
        break;
    case Vt52ActionCodes::ExitAlternateKeypadMode:
        _dispatch->SetKeypadMode(false);
        break;
    case Vt52ActionCodes::ExitVt52Mode:
        _dispatch->SetMode(DispatchTypes::ModeParams::DECANM_AnsiMode);
        break;
    default:
        break;
    }

    _ClearLastChar();

    return true;
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
    // Bail out if we receive subparameters, but we don't accept them in the sequence.
    if (parameters.hasSubParams() && !_CanSeqAcceptSubParam(id, parameters)) [[unlikely]]
    {
        return true;
    }

    switch (id)
    {
    case CsiActionCodes::CUU_CursorUp:
        _dispatch->CursorUp(parameters.at(0));
        break;
    case CsiActionCodes::CUD_CursorDown:
        _dispatch->CursorDown(parameters.at(0));
        break;
    case CsiActionCodes::CUF_CursorForward:
        _dispatch->CursorForward(parameters.at(0));
        break;
    case CsiActionCodes::CUB_CursorBackward:
        _dispatch->CursorBackward(parameters.at(0));
        break;
    case CsiActionCodes::CNL_CursorNextLine:
        _dispatch->CursorNextLine(parameters.at(0));
        break;
    case CsiActionCodes::CPL_CursorPrevLine:
        _dispatch->CursorPrevLine(parameters.at(0));
        break;
    case CsiActionCodes::CHA_CursorHorizontalAbsolute:
    case CsiActionCodes::HPA_HorizontalPositionAbsolute:
        _dispatch->CursorHorizontalPositionAbsolute(parameters.at(0));
        break;
    case CsiActionCodes::VPA_VerticalLinePositionAbsolute:
        _dispatch->VerticalLinePositionAbsolute(parameters.at(0));
        break;
    case CsiActionCodes::HPR_HorizontalPositionRelative:
        _dispatch->HorizontalPositionRelative(parameters.at(0));
        break;
    case CsiActionCodes::VPR_VerticalPositionRelative:
        _dispatch->VerticalPositionRelative(parameters.at(0));
        break;
    case CsiActionCodes::CUP_CursorPosition:
    case CsiActionCodes::HVP_HorizontalVerticalPosition:
        _dispatch->CursorPosition(parameters.at(0), parameters.at(1));
        break;
    case CsiActionCodes::DECSTBM_SetTopBottomMargins:
        _dispatch->SetTopBottomScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
        break;
    case CsiActionCodes::DECSLRM_SetLeftRightMargins:
        // Note that this can also be ANSISYSSC, depending on the state of DECLRMM.
        _dispatch->SetLeftRightScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
        break;
    case CsiActionCodes::ICH_InsertCharacter:
        _dispatch->InsertCharacter(parameters.at(0));
        break;
    case CsiActionCodes::DCH_DeleteCharacter:
        _dispatch->DeleteCharacter(parameters.at(0));
        break;
    case CsiActionCodes::ED_EraseDisplay:
        parameters.for_each([&](const auto eraseType) {
            _dispatch->EraseInDisplay(eraseType);
        });
        break;
    case CsiActionCodes::DECSED_SelectiveEraseDisplay:
        parameters.for_each([&](const auto eraseType) {
            _dispatch->SelectiveEraseInDisplay(eraseType);
        });
        break;
    case CsiActionCodes::EL_EraseLine:
        parameters.for_each([&](const auto eraseType) {
            _dispatch->EraseInLine(eraseType);
        });
        break;
    case CsiActionCodes::DECSEL_SelectiveEraseLine:
        parameters.for_each([&](const auto eraseType) {
            _dispatch->SelectiveEraseInLine(eraseType);
        });
        break;
    case CsiActionCodes::SM_SetMode:
        parameters.for_each([&](const auto mode) {
            _dispatch->SetMode(DispatchTypes::ANSIStandardMode(mode));
        });
        break;
    case CsiActionCodes::DECSET_PrivateModeSet:
        parameters.for_each([&](const auto mode) {
            _dispatch->SetMode(DispatchTypes::DECPrivateMode(mode));
        });
        break;
    case CsiActionCodes::RM_ResetMode:
        parameters.for_each([&](const auto mode) {
            _dispatch->ResetMode(DispatchTypes::ANSIStandardMode(mode));
        });
        break;
    case CsiActionCodes::DECRST_PrivateModeReset:
        parameters.for_each([&](const auto mode) {
            _dispatch->ResetMode(DispatchTypes::DECPrivateMode(mode));
        });
        break;
    case CsiActionCodes::SGR_SetGraphicsRendition:
        _dispatch->SetGraphicsRendition(parameters);
        break;
    case CsiActionCodes::DSR_DeviceStatusReport:
        _dispatch->DeviceStatusReport(DispatchTypes::ANSIStandardStatus(parameters.at(0)), parameters.at(1));
        break;
    case CsiActionCodes::DSR_PrivateDeviceStatusReport:
        _dispatch->DeviceStatusReport(DispatchTypes::DECPrivateStatus(parameters.at(0)), parameters.at(1));
        break;
    case CsiActionCodes::DA_DeviceAttributes:
        if (parameters.at(0).value_or(0) == 0)
        {
            _dispatch->DeviceAttributes();
        }
        break;
    case CsiActionCodes::DA2_SecondaryDeviceAttributes:
        if (parameters.at(0).value_or(0) == 0)
        {
            _dispatch->SecondaryDeviceAttributes();
        }
        break;
    case CsiActionCodes::DA3_TertiaryDeviceAttributes:
        if (parameters.at(0).value_or(0) == 0)
        {
            _dispatch->TertiaryDeviceAttributes();
        }
        break;
    case CsiActionCodes::DECREQTPARM_RequestTerminalParameters:
        _dispatch->RequestTerminalParameters(parameters.at(0));
        break;
    case CsiActionCodes::SU_ScrollUp:
        _dispatch->ScrollUp(parameters.at(0));
        break;
    case CsiActionCodes::SD_ScrollDown:
        _dispatch->ScrollDown(parameters.at(0));
        break;
    case CsiActionCodes::NP_NextPage:
        _dispatch->NextPage(parameters.at(0));
        break;
    case CsiActionCodes::PP_PrecedingPage:
        _dispatch->PrecedingPage(parameters.at(0));
        break;
    case CsiActionCodes::ANSISYSRC_CursorRestore:
        _dispatch->CursorRestoreState();
        break;
    case CsiActionCodes::IL_InsertLine:
        _dispatch->InsertLine(parameters.at(0));
        break;
    case CsiActionCodes::DL_DeleteLine:
        _dispatch->DeleteLine(parameters.at(0));
        break;
    case CsiActionCodes::CHT_CursorForwardTab:
        _dispatch->ForwardTab(parameters.at(0));
        break;
    case CsiActionCodes::CBT_CursorBackTab:
        _dispatch->BackwardsTab(parameters.at(0));
        break;
    case CsiActionCodes::TBC_TabClear:
        parameters.for_each([&](const auto clearType) {
            _dispatch->TabClear(clearType);
        });
        break;
    case CsiActionCodes::DECST8C_SetTabEvery8Columns:
        parameters.for_each([&](const auto setType) {
            _dispatch->TabSet(setType);
        });
        break;
    case CsiActionCodes::ECH_EraseCharacters:
        _dispatch->EraseCharacters(parameters.at(0));
        break;
    case CsiActionCodes::DTTERM_WindowManipulation:
        _dispatch->WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
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
        break;
    case CsiActionCodes::PPA_PagePositionAbsolute:
        _dispatch->PagePositionAbsolute(parameters.at(0));
        break;
    case CsiActionCodes::PPR_PagePositionRelative:
        _dispatch->PagePositionRelative(parameters.at(0));
        break;
    case CsiActionCodes::PPB_PagePositionBack:
        _dispatch->PagePositionBack(parameters.at(0));
        break;
    case CsiActionCodes::DECSCUSR_SetCursorStyle:
        _dispatch->SetCursorStyle(parameters.at(0));
        break;
    case CsiActionCodes::DECSTR_SoftReset:
        _dispatch->SoftReset();
        break;
    case CsiActionCodes::DECSCA_SetCharacterProtectionAttribute:
        _dispatch->SetCharacterProtectionAttribute(parameters);
        break;
    case CsiActionCodes::DECRQDE_RequestDisplayedExtent:
        _dispatch->RequestDisplayedExtent();
        break;
    case CsiActionCodes::XT_PushSgr:
    case CsiActionCodes::XT_PushSgrAlias:
        _dispatch->PushGraphicsRendition(parameters);
        break;
    case CsiActionCodes::XT_PopSgr:
    case CsiActionCodes::XT_PopSgrAlias:
        _dispatch->PopGraphicsRendition();
        break;
    case CsiActionCodes::DECRQM_RequestMode:
        _dispatch->RequestMode(DispatchTypes::ANSIStandardMode(parameters.at(0)));
        break;
    case CsiActionCodes::DECRQM_PrivateRequestMode:
        _dispatch->RequestMode(DispatchTypes::DECPrivateMode(parameters.at(0)));
        break;
    case CsiActionCodes::DECCARA_ChangeAttributesRectangularArea:
        _dispatch->ChangeAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        break;
    case CsiActionCodes::DECRARA_ReverseAttributesRectangularArea:
        _dispatch->ReverseAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        break;
    case CsiActionCodes::DECCRA_CopyRectangularArea:
        _dispatch->CopyRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.at(4), parameters.at(5), parameters.at(6), parameters.at(7));
        break;
    case CsiActionCodes::DECRQTSR_RequestTerminalStateReport:
        _dispatch->RequestTerminalStateReport(parameters.at(0), parameters.at(1));
        break;
    case CsiActionCodes::DECRQPSR_RequestPresentationStateReport:
        _dispatch->RequestPresentationStateReport(parameters.at(0));
        break;
    case CsiActionCodes::DECFRA_FillRectangularArea:
        _dispatch->FillRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2), parameters.at(3).value_or(0), parameters.at(4).value_or(0));
        break;
    case CsiActionCodes::DECERA_EraseRectangularArea:
        _dispatch->EraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        break;
    case CsiActionCodes::DECSERA_SelectiveEraseRectangularArea:
        _dispatch->SelectiveEraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        break;
    case CsiActionCodes::DECRQUPSS_RequestUserPreferenceSupplementalSet:
        _dispatch->RequestUserPreferenceCharset();
        break;
    case CsiActionCodes::DECIC_InsertColumn:
        _dispatch->InsertColumn(parameters.at(0));
        break;
    case CsiActionCodes::DECDC_DeleteColumn:
        _dispatch->DeleteColumn(parameters.at(0));
        break;
    case CsiActionCodes::DECSACE_SelectAttributeChangeExtent:
        _dispatch->SelectAttributeChangeExtent(parameters.at(0));
        break;
    case CsiActionCodes::DECRQCRA_RequestChecksumRectangularArea:
        _dispatch->RequestChecksumRectangularArea(parameters.at(0).value_or(0), parameters.at(1).value_or(0), parameters.at(2), parameters.at(3), parameters.at(4).value_or(0), parameters.at(5).value_or(0));
        break;
    case CsiActionCodes::DECINVM_InvokeMacro:
        _dispatch->InvokeMacro(parameters.at(0).value_or(0));
        break;
    case CsiActionCodes::DECAC_AssignColor:
        _dispatch->AssignColor(parameters.at(0), parameters.at(1).value_or(0), parameters.at(2).value_or(0));
        break;
    case CsiActionCodes::DECPS_PlaySound:
        _dispatch->PlaySounds(parameters);
        break;
    default:
        break;
    }

    _ClearLastChar();

    return true;
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
IStateMachineEngine::StringHandler OutputStateMachineEngine::ActionDcsDispatch(const VTID id, const VTParameters parameters)
{
    StringHandler handler = nullptr;

    switch (id)
    {
    case DcsActionCodes::SIXEL_DefineImage:
        handler = _dispatch->DefineSixelImage(parameters.at(0),
                                              parameters.at(1),
                                              parameters.at(2));
        break;
    case DcsActionCodes::DECDLD_DownloadDRCS:
        handler = _dispatch->DownloadDRCS(parameters.at(0),
                                          parameters.at(1),
                                          parameters.at(2),
                                          parameters.at(3),
                                          parameters.at(4),
                                          parameters.at(5),
                                          parameters.at(6),
                                          parameters.at(7));
        break;
    case DcsActionCodes::DECAUPSS_AssignUserPreferenceSupplementalSet:
        handler = _dispatch->AssignUserPreferenceCharset(parameters.at(0));
        break;
    case DcsActionCodes::DECDMAC_DefineMacro:
        handler = _dispatch->DefineMacro(parameters.at(0).value_or(0), parameters.at(1), parameters.at(2));
        break;
    case DcsActionCodes::DECRSTS_RestoreTerminalState:
        handler = _dispatch->RestoreTerminalState(parameters.at(0));
        break;
    case DcsActionCodes::DECRQSS_RequestSetting:
        handler = _dispatch->RequestSetting();
        break;
    case DcsActionCodes::DECRSPS_RestorePresentationState:
        handler = _dispatch->RestorePresentationState(parameters.at(0));
        break;
    default:
        handler = nullptr;
        break;
    }

    _ClearLastChar();

    return handler;
}

// Routine Description:
// - Triggers the OscDispatch action to indicate that the listener should handle a control sequence.
//   These sequences perform various API-type commands that can include many parameters.
// Arguments:
// - parameter - identifier of the OSC action to perform
// - string - OSC string we've collected. NOT null terminated.
// Return Value:
// - true if we handled the dispatch.
bool OutputStateMachineEngine::ActionOscDispatch(const size_t parameter, const std::wstring_view string)
{
    switch (parameter)
    {
    case OscActionCodes::SetIconAndWindowTitle:
    case OscActionCodes::SetWindowIcon:
    case OscActionCodes::SetWindowTitle:
    case OscActionCodes::DECSWT_SetWindowTitle:
    {
        _dispatch->SetWindowTitle(string);
        break;
    }
    case OscActionCodes::SetColor:
    {
        std::vector<size_t> tableIndexes;
        std::vector<DWORD> colors;
        if (_GetOscSetColorTable(string, tableIndexes, colors))
        {
            for (size_t i = 0; i < tableIndexes.size(); i++)
            {
                const auto tableIndex = til::at(tableIndexes, i);
                const auto rgb = til::at(colors, i);
                if (rgb == COLOR_INQUIRY_COLOR)
                {
                    _dispatch->RequestColorTableEntry(tableIndex);
                }
                else
                {
                    _dispatch->SetColorTableEntry(tableIndex, rgb);
                }
            }
        }
        break;
    }
    case OscActionCodes::SetForegroundColor:
    case OscActionCodes::SetBackgroundColor:
    case OscActionCodes::SetCursorColor:
    case OscActionCodes::SetHighlightColor:
    {
        std::vector<DWORD> colors;
        if (_GetOscSetColor(string, colors))
        {
            auto resource = parameter;
            for (auto&& color : colors)
            {
                if (color == COLOR_INQUIRY_COLOR)
                {
                    _dispatch->RequestXtermColorResource(resource);
                }
                else if (color != INVALID_COLOR)
                {
                    _dispatch->SetXtermColorResource(resource, color);
                }
                resource++;
            }
        }
        break;
    }
    case OscActionCodes::SetClipboard:
    {
        std::wstring setClipboardContent;
        auto queryClipboard = false;
        if (_GetOscSetClipboard(string, setClipboardContent, queryClipboard) && !queryClipboard)
        {
            _dispatch->SetClipboard(setClipboardContent);
        }
        break;
    }
    case OscActionCodes::ResetCursorColor:
    {
        // The reset codes for xterm dynamic resources are the set codes + 100
        _dispatch->SetXtermColorResource(parameter - 100u, INVALID_COLOR);
        break;
    }
    case OscActionCodes::Hyperlink:
    {
        std::wstring params;
        std::wstring uri;
        if (_ParseHyperlink(string, params, uri))
        {
            if (uri.empty())
            {
                _dispatch->EndHyperlink();
            }
            else
            {
                _dispatch->AddHyperlink(uri, params);
            }
        }
        break;
    }
    case OscActionCodes::ConEmuAction:
    {
        _dispatch->DoConEmuAction(string);
        break;
    }
    case OscActionCodes::ITerm2Action:
    {
        _dispatch->DoITerm2Action(string);
        break;
    }
    case OscActionCodes::FinalTermAction:
    {
        _dispatch->DoFinalTermAction(string);
        break;
    }
    case OscActionCodes::VsCodeAction:
    {
        _dispatch->DoVsCodeAction(string);
        break;
    }
    case OscActionCodes::WTAction:
    {
        _dispatch->DoWTAction(string);
        break;
    }
    default:
        break;
    }

    _ClearLastChar();

    return true;
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
    return true;
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
                                                    std::vector<DWORD>& rgbs) const
{
    using namespace std::string_view_literals;

    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 2)
    {
        return false;
    }

    std::vector<size_t> newTableIndexes;
    std::vector<DWORD> newRgbs;

    for (size_t i = 0, j = 1; j < parts.size(); i += 2, j += 2)
    {
        auto&& index = til::at(parts, i);
        auto&& color = til::at(parts, j);
        unsigned int tableIndex = 0;
        const auto indexSuccess = Utils::StringToUint(index, tableIndex);

        if (indexSuccess)
        {
            if (color == L"?"sv) [[unlikely]]
            {
                newTableIndexes.push_back(tableIndex);
                newRgbs.push_back(COLOR_INQUIRY_COLOR);
            }
            else if (const auto colorOptional = Utils::ColorFromXTermColor(color))
            {
                newTableIndexes.push_back(tableIndex);
                newRgbs.push_back(colorOptional.value());
            }
        }
    }

    tableIndexes.swap(newTableIndexes);
    rgbs.swap(newRgbs);

    return tableIndexes.size() > 0 && rgbs.size() > 0;
}

#pragma warning(push)
#pragma warning(disable : 26445) // Suppress lifetime check for a reference to std::span or std::string_view

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

    const auto midPos = string.find(';');
    if (midPos != std::wstring::npos)
    {
        uri = string.substr(midPos + 1, MAX_URL_LENGTH);
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
//   It's possible to have multiple "spec", which by design equals a series of OSC command
//   with accumulated Ps. For example "OSC 10;color1;color2" is effectively an "OSC 10;color1"
//   and an "OSC 11;color2".
//
// Arguments:
// - string - the Osc String to parse
// - rgbs - receives the colors that we parsed in the format: 0x00BBGGRR
// Return Value:
// - True if at least one color was parsed successfully. False otherwise.
bool OutputStateMachineEngine::_GetOscSetColor(const std::wstring_view string,
                                               std::vector<DWORD>& rgbs) const
{
    using namespace std::string_view_literals;

    const auto parts = Utils::SplitString(string, L';');
    if (parts.size() < 1)
    {
        return false;
    }

    std::vector<DWORD> newRgbs;
    for (const auto& part : parts)
    {
        if (part == L"?"sv) [[unlikely]]
        {
            newRgbs.push_back(COLOR_INQUIRY_COLOR);
            continue;
        }
        else if (const auto colorOptional = Utils::ColorFromXTermColor(part))
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
    const auto pos = string.find(L';');
    if (pos == std::wstring_view::npos)
    {
        return false;
    }

    const auto substr = string.substr(pos + 1);
    if (substr == L"?")
    {
        queryClipboard = true;
        return true;
    }

// Log_IfFailed has the following description: "Should be decorated WI_NOEXCEPT, but conflicts with forceinline."
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'Log_IfFailed()' which may throw exceptions (f.6).
    return SUCCEEDED_LOG(Base64::Decode(substr, content));
}

// Routine Description:
// - Takes a sequence id ("final byte") and determines if it accepts sub parameters.
// Arguments:
// - id - The sequence id to check for.
// Return Value:
// - True, if it accepts sub parameters or else False.
bool OutputStateMachineEngine::_CanSeqAcceptSubParam(const VTID id, const VTParameters& parameters) noexcept
{
    switch (id)
    {
    case SGR_SetGraphicsRendition:
        return true;
    case DECCARA_ChangeAttributesRectangularArea:
    case DECRARA_ReverseAttributesRectangularArea:
        return !parameters.hasSubParamsFor(0) && !parameters.hasSubParamsFor(1) && !parameters.hasSubParamsFor(2) && !parameters.hasSubParamsFor(3);
    default:
        return false;
    }
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
