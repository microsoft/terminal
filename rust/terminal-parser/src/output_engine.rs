//! Safe Rust implementation of Windows Terminal's VT output state-machine engine.
//!
//! The parser owns VT grammar and incremental state. This module translates the
//! parser's actions into typed terminal-dispatch operations without depending on
//! conhost, `WinRT`, COM, or C++.

use crate::base64;
use crate::state_machine::{Parameters, StateMachineEngine, VtId};

const NUL: u16 = 0x00;
const ENQ: u16 = 0x05;
const BEL: u16 = 0x07;
const BS: u16 = 0x08;
const TAB: u16 = 0x09;
const LF: u16 = 0x0a;
const VT: u16 = 0x0b;
const FF: u16 = 0x0c;
const CR: u16 = 0x0d;
const SO: u16 = 0x0e;
const SI: u16 = 0x0f;
const SUB: u16 = 0x1a;
const DEL: u16 = 0x7f;

/// Maximum hyperlink URI length accepted by Windows Terminal's output engine.
pub const MAX_URL_LENGTH: usize = 2 * 1_048_576;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LineFeedType {
    DependsOnMode,
    WithReturn,
    WithoutReturn,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LineRendition {
    SingleWidth,
    DoubleWidth,
    DoubleHeightTop,
    DoubleHeightBottom,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceAttributesKind {
    Primary,
    Secondary,
    Tertiary,
    Vt52,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DcsAction {
    DefineSixelImage(Parameters),
    DownloadDrcs(Parameters),
    AssignUserPreferenceCharset(Parameters),
    DefineMacro(Parameters),
    RestoreTerminalState(Parameters),
    RequestSetting,
    RestorePresentationState(Parameters),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum OutputAction {
    UnknownSequence,
    Print(u16),
    PrintString(Vec<u16>),
    EnquireAnswerback,
    WarningBell,
    CursorBackward(i32),
    ForwardTab(i32),
    BackwardsTab(i32),
    CarriageReturn,
    LineFeed(LineFeedType),
    ReverseLineFeed,
    LockingShift(u8),
    LockingShiftRight(u8),
    SingleShift(u8),
    BackIndex,
    ForwardIndex,
    CursorSaveState,
    CursorRestoreState,
    SetKeypadMode(bool),
    HorizontalTabSet,
    AcceptC1Controls(bool),
    SendC1Controls(bool),
    AnnounceCodeStructure(u8),
    SetLineRendition(LineRendition),
    ScreenAlignmentPattern,
    DesignateCodingSystem(u64),
    Designate94Charset {
        slot: u8,
        charset: u64,
    },
    Designate96Charset {
        slot: u8,
        charset: u64,
    },
    HardReset,
    CursorUp(i32),
    CursorDown(i32),
    CursorForward(i32),
    CursorNextLine(i32),
    CursorPreviousLine(i32),
    CursorHorizontalPositionAbsolute(i32),
    VerticalLinePositionAbsolute(i32),
    HorizontalPositionRelative(i32),
    VerticalPositionRelative(i32),
    CursorPosition {
        line: i32,
        column: i32,
    },
    SetTopBottomScrollingMargins {
        top: i32,
        bottom: i32,
    },
    SetLeftRightScrollingMargins {
        left: i32,
        right: i32,
    },
    InsertCharacter(i32),
    DeleteCharacter(i32),
    InsertLine(i32),
    DeleteLine(i32),
    EraseInDisplay(i32),
    SelectiveEraseInDisplay(i32),
    EraseInLine(i32),
    SelectiveEraseInLine(i32),
    EraseCharacters(i32),
    SetMode {
        private: bool,
        enabled: bool,
        mode: i32,
    },
    SetGraphicsRendition(Parameters),
    DeviceStatusReport {
        private: bool,
        status: i32,
        id: Option<i32>,
    },
    DeviceAttributes(DeviceAttributesKind),
    RequestTerminalParameters(i32),
    ScrollUp(i32),
    ScrollDown(i32),
    NextPage(i32),
    PrecedingPage(i32),
    TabClear(i32),
    TabSet(i32),
    WindowManipulation {
        function: i32,
        parameter1: i32,
        parameter2: i32,
    },
    PagePositionAbsolute(i32),
    PagePositionRelative(i32),
    PagePositionBack(i32),
    SetCursorStyle(i32),
    SoftReset,
    SetCharacterProtectionAttribute(Parameters),
    RequestDisplayedExtent,
    PushGraphicsRendition(Parameters),
    PopGraphicsRendition,
    RequestMode {
        private: bool,
        mode: i32,
    },
    AdvancedCsi {
        id: VtId,
        parameters: Parameters,
    },
    SetWindowTitle(String),
    SetCurrentWorkingDirectory(String),
    SetClipboard(String),
    AddHyperlink {
        uri: String,
        custom_id: String,
    },
    EndHyperlink,
    SetColorTableEntry {
        index: usize,
        color: u32,
    },
    RequestColorTableEntry(usize),
    ResetColorTable,
    ResetColorTableEntry(usize),
    SetXtermColorResource {
        resource: usize,
        color: u32,
    },
    RequestXtermColorResource(usize),
    ResetXtermColorResource(usize),
    ConEmuAction(String),
    FinalTermAction(String),
    VsCodeAction(String),
    UrxvtAction(String),
    ITerm2Action(String),
    WtAction(String),
    DcsBegin(DcsAction),
    DcsPut(u16),
}

/// Destination for semantic output-engine operations.
///
/// A future terminal-core or FFI adapter can implement this trait without
/// changing the parser or output-engine contract.
pub trait TermDispatch {
    fn dispatch(&mut self, action: OutputAction);

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.dispatch(OutputAction::DcsBegin(action));
        false
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        self.dispatch(OutputAction::DcsPut(code_unit));
        true
    }
}

pub struct OutputStateMachineEngine<D: TermDispatch> {
    dispatch: D,
    last_printed_char: u16,
}

impl<D: TermDispatch> OutputStateMachineEngine<D> {
    #[must_use]
    pub const fn new(dispatch: D) -> Self {
        Self {
            dispatch,
            last_printed_char: NUL,
        }
    }

    #[must_use]
    pub const fn dispatch(&self) -> &D {
        &self.dispatch
    }

    pub const fn dispatch_mut(&mut self) -> &mut D {
        &mut self.dispatch
    }

    #[must_use]
    pub fn into_dispatch(self) -> D {
        self.dispatch
    }

    fn emit(&mut self, action: OutputAction) {
        self.dispatch.dispatch(action);
    }

    fn clear_last_char(&mut self) {
        self.last_printed_char = NUL;
    }

    fn action_esc(&mut self, id: VtId) {
        let action = if id_is(id, "\\") {
            None
        } else if id_is(id, "6") {
            Some(OutputAction::BackIndex)
        } else if id_is(id, "7") {
            Some(OutputAction::CursorSaveState)
        } else if id_is(id, "8") {
            Some(OutputAction::CursorRestoreState)
        } else if id_is(id, "9") {
            Some(OutputAction::ForwardIndex)
        } else if id_is(id, "=") {
            Some(OutputAction::SetKeypadMode(true))
        } else if id_is(id, ">") {
            Some(OutputAction::SetKeypadMode(false))
        } else if id_is(id, "D") {
            Some(OutputAction::LineFeed(LineFeedType::WithoutReturn))
        } else if id_is(id, "E") {
            Some(OutputAction::LineFeed(LineFeedType::WithReturn))
        } else if id_is(id, "H") {
            Some(OutputAction::HorizontalTabSet)
        } else if id_is(id, "M") {
            Some(OutputAction::ReverseLineFeed)
        } else if id_is(id, "N") {
            Some(OutputAction::SingleShift(2))
        } else if id_is(id, "O") {
            Some(OutputAction::SingleShift(3))
        } else if id_is(id, "Z") {
            Some(OutputAction::DeviceAttributes(
                DeviceAttributesKind::Primary,
            ))
        } else if id_is(id, "c") {
            Some(OutputAction::HardReset)
        } else if id_is(id, "n") {
            Some(OutputAction::LockingShift(2))
        } else if id_is(id, "o") {
            Some(OutputAction::LockingShift(3))
        } else if id_is(id, "~") {
            Some(OutputAction::LockingShiftRight(1))
        } else if id_is(id, "}") {
            Some(OutputAction::LockingShiftRight(2))
        } else if id_is(id, "|") {
            Some(OutputAction::LockingShiftRight(3))
        } else if id_is(id, " 7") {
            Some(OutputAction::AcceptC1Controls(true))
        } else if id_is(id, " F") {
            Some(OutputAction::SendC1Controls(false))
        } else if id_is(id, " G") {
            Some(OutputAction::SendC1Controls(true))
        } else if id_is(id, " L") {
            Some(OutputAction::AnnounceCodeStructure(1))
        } else if id_is(id, " M") {
            Some(OutputAction::AnnounceCodeStructure(2))
        } else if id_is(id, " N") {
            Some(OutputAction::AnnounceCodeStructure(3))
        } else if id_is(id, "#3") {
            Some(OutputAction::SetLineRendition(
                LineRendition::DoubleHeightTop,
            ))
        } else if id_is(id, "#4") {
            Some(OutputAction::SetLineRendition(
                LineRendition::DoubleHeightBottom,
            ))
        } else if id_is(id, "#5") {
            Some(OutputAction::SetLineRendition(LineRendition::SingleWidth))
        } else if id_is(id, "#6") {
            Some(OutputAction::SetLineRendition(LineRendition::DoubleWidth))
        } else if id_is(id, "#8") {
            Some(OutputAction::ScreenAlignmentPattern)
        } else {
            charset_action(id)
        };

        if let Some(action) = action {
            self.emit(action);
        }
        self.clear_last_char();
    }

    fn action_vt52(&mut self, id: VtId, parameters: &Parameters) {
        let action = if id_is(id, "A") {
            Some(OutputAction::CursorUp(1))
        } else if id_is(id, "B") {
            Some(OutputAction::CursorDown(1))
        } else if id_is(id, "C") {
            Some(OutputAction::CursorForward(1))
        } else if id_is(id, "D") {
            Some(OutputAction::CursorBackward(1))
        } else if id_is(id, "F") {
            Some(OutputAction::Designate94Charset {
                slot: 0,
                charset: u64::from(b'0'),
            })
        } else if id_is(id, "G") {
            Some(OutputAction::Designate94Charset {
                slot: 0,
                charset: u64::from(b'B'),
            })
        } else if id_is(id, "H") {
            Some(OutputAction::CursorPosition { line: 1, column: 1 })
        } else if id_is(id, "I") {
            Some(OutputAction::ReverseLineFeed)
        } else if id_is(id, "J") {
            Some(OutputAction::EraseInDisplay(0))
        } else if id_is(id, "K") {
            Some(OutputAction::EraseInLine(0))
        } else if id_is(id, "Y") {
            Some(OutputAction::CursorPosition {
                line: parameters.at(0).unwrap_or(0) - i32::from(b' ') + 1,
                column: parameters.at(1).unwrap_or(0) - i32::from(b' ') + 1,
            })
        } else if id_is(id, "Z") {
            Some(OutputAction::DeviceAttributes(DeviceAttributesKind::Vt52))
        } else if id_is(id, "=") {
            Some(OutputAction::SetKeypadMode(true))
        } else if id_is(id, ">") {
            Some(OutputAction::SetKeypadMode(false))
        } else if id_is(id, "<") {
            Some(OutputAction::SetMode {
                private: true,
                enabled: true,
                mode: 2,
            })
        } else {
            None
        };

        if let Some(action) = action {
            self.emit(action);
        }
        self.clear_last_char();
    }

    #[expect(
        clippy::too_many_lines,
        reason = "CSI dispatch is kept as a contiguous protocol table for Microsoft parity review"
    )]
    fn action_csi(&mut self, id: VtId, parameters: &Parameters) {
        if has_sub_params(parameters) && !can_accept_sub_params(id, parameters) {
            self.clear_last_char();
            return;
        }

        if id_is(id, "A") {
            self.emit(OutputAction::CursorUp(numeric(parameters, 0)));
        } else if id_is(id, "B") {
            self.emit(OutputAction::CursorDown(numeric(parameters, 0)));
        } else if id_is(id, "C") {
            self.emit(OutputAction::CursorForward(numeric(parameters, 0)));
        } else if id_is(id, "D") {
            self.emit(OutputAction::CursorBackward(numeric(parameters, 0)));
        } else if id_is(id, "E") {
            self.emit(OutputAction::CursorNextLine(numeric(parameters, 0)));
        } else if id_is(id, "F") {
            self.emit(OutputAction::CursorPreviousLine(numeric(parameters, 0)));
        } else if id_is(id, "G") || id_is(id, "`") {
            self.emit(OutputAction::CursorHorizontalPositionAbsolute(numeric(
                parameters, 0,
            )));
        } else if id_is(id, "d") {
            self.emit(OutputAction::VerticalLinePositionAbsolute(numeric(
                parameters, 0,
            )));
        } else if id_is(id, "a") {
            self.emit(OutputAction::HorizontalPositionRelative(numeric(
                parameters, 0,
            )));
        } else if id_is(id, "e") {
            self.emit(OutputAction::VerticalPositionRelative(numeric(
                parameters, 0,
            )));
        } else if id_is(id, "H") || id_is(id, "f") {
            self.emit(OutputAction::CursorPosition {
                line: numeric(parameters, 0),
                column: numeric(parameters, 1),
            });
        } else if id_is(id, "r") {
            self.emit(OutputAction::SetTopBottomScrollingMargins {
                top: raw_or(parameters, 0, 0),
                bottom: raw_or(parameters, 1, 0),
            });
        } else if id_is(id, "s") {
            self.emit(OutputAction::SetLeftRightScrollingMargins {
                left: raw_or(parameters, 0, 0),
                right: raw_or(parameters, 1, 0),
            });
        } else if id_is(id, "@") {
            self.emit(OutputAction::InsertCharacter(numeric(parameters, 0)));
        } else if id_is(id, "P") {
            self.emit(OutputAction::DeleteCharacter(numeric(parameters, 0)));
        } else if id_is(id, "L") {
            self.emit(OutputAction::InsertLine(numeric(parameters, 0)));
        } else if id_is(id, "M") {
            self.emit(OutputAction::DeleteLine(numeric(parameters, 0)));
        } else if id_is(id, "J") {
            for_each_parameter(parameters, |value| {
                self.emit(OutputAction::EraseInDisplay(value));
            });
        } else if id_is(id, "?J") {
            for_each_parameter(parameters, |value| {
                self.emit(OutputAction::SelectiveEraseInDisplay(value));
            });
        } else if id_is(id, "K") {
            for_each_parameter(parameters, |value| {
                self.emit(OutputAction::EraseInLine(value));
            });
        } else if id_is(id, "?K") {
            for_each_parameter(parameters, |value| {
                self.emit(OutputAction::SelectiveEraseInLine(value));
            });
        } else if id_is(id, "h") || id_is(id, "?h") || id_is(id, "l") || id_is(id, "?l") {
            let private = id_is(id, "?h") || id_is(id, "?l");
            let enabled = id_is(id, "h") || id_is(id, "?h");
            for_each_parameter(parameters, |mode| {
                self.emit(OutputAction::SetMode {
                    private,
                    enabled,
                    mode,
                });
            });
        } else if id_is(id, "m") {
            self.emit(OutputAction::SetGraphicsRendition(parameters.clone()));
        } else if id_is(id, "n") || id_is(id, "?n") {
            self.emit(OutputAction::DeviceStatusReport {
                private: id_is(id, "?n"),
                status: raw_or(parameters, 0, 0),
                id: parameters.at(1),
            });
        } else if id_is(id, "c") || id_is(id, ">c") || id_is(id, "=c") {
            if raw_or(parameters, 0, 0) == 0 {
                let kind = if id_is(id, "c") {
                    DeviceAttributesKind::Primary
                } else if id_is(id, ">c") {
                    DeviceAttributesKind::Secondary
                } else {
                    DeviceAttributesKind::Tertiary
                };
                self.emit(OutputAction::DeviceAttributes(kind));
            }
        } else if id_is(id, "x") {
            self.emit(OutputAction::RequestTerminalParameters(raw_or(
                parameters, 0, 0,
            )));
        } else if id_is(id, "S") {
            self.emit(OutputAction::ScrollUp(numeric(parameters, 0)));
        } else if id_is(id, "T") {
            self.emit(OutputAction::ScrollDown(numeric(parameters, 0)));
        } else if id_is(id, "U") {
            self.emit(OutputAction::NextPage(numeric(parameters, 0)));
        } else if id_is(id, "V") {
            self.emit(OutputAction::PrecedingPage(numeric(parameters, 0)));
        } else if id_is(id, "I") {
            self.emit(OutputAction::ForwardTab(numeric(parameters, 0)));
        } else if id_is(id, "Z") {
            self.emit(OutputAction::BackwardsTab(numeric(parameters, 0)));
        } else if id_is(id, "g") {
            for_each_parameter(parameters, |value| self.emit(OutputAction::TabClear(value)));
        } else if id_is(id, "?W") {
            for_each_parameter(parameters, |value| self.emit(OutputAction::TabSet(value)));
        } else if id_is(id, "X") {
            self.emit(OutputAction::EraseCharacters(numeric(parameters, 0)));
        } else if id_is(id, "t") {
            self.emit(OutputAction::WindowManipulation {
                function: numeric(parameters, 0),
                parameter1: numeric(parameters, 1),
                parameter2: numeric(parameters, 2),
            });
        } else if id_is(id, "b") {
            if self.last_printed_char != NUL {
                let count = usize::try_from(numeric(parameters, 0)).unwrap_or_default();
                self.emit(OutputAction::PrintString(vec![
                    self.last_printed_char;
                    count
                ]));
            }
        } else if id_is(id, "u") {
            self.emit(OutputAction::CursorRestoreState);
        } else if id_is(id, " P") {
            self.emit(OutputAction::PagePositionAbsolute(numeric(parameters, 0)));
        } else if id_is(id, " Q") {
            self.emit(OutputAction::PagePositionRelative(numeric(parameters, 0)));
        } else if id_is(id, " R") {
            self.emit(OutputAction::PagePositionBack(numeric(parameters, 0)));
        } else if id_is(id, " q") {
            self.emit(OutputAction::SetCursorStyle(raw_or(parameters, 0, 0)));
        } else if id_is(id, "!p") {
            self.emit(OutputAction::SoftReset);
        } else if id_is(id, "\"q") {
            self.emit(OutputAction::SetCharacterProtectionAttribute(
                parameters.clone(),
            ));
        } else if id_is(id, "\"v") {
            self.emit(OutputAction::RequestDisplayedExtent);
        } else if id_is(id, "#{") || id_is(id, "#p") {
            self.emit(OutputAction::PushGraphicsRendition(parameters.clone()));
        } else if id_is(id, "#}") || id_is(id, "#q") {
            self.emit(OutputAction::PopGraphicsRendition);
        } else if id_is(id, "$p") || id_is(id, "?$p") {
            self.emit(OutputAction::RequestMode {
                private: id_is(id, "?$p"),
                mode: raw_or(parameters, 0, 0),
            });
        } else if is_recognized_advanced_csi(id) {
            self.emit(OutputAction::AdvancedCsi {
                id,
                parameters: parameters.clone(),
            });
        } else {
            self.emit(OutputAction::UnknownSequence);
        }

        self.clear_last_char();
    }

    fn action_osc(&mut self, parameter: i32, text: &[u16]) {
        match parameter {
            0 | 1 | 2 | 21 => self.emit(OutputAction::SetWindowTitle(utf16_lossy(text))),
            4 => self.osc_color_table(text),
            10 | 11 | 12 | 17 => self.osc_dynamic_colors(parameter, text),
            52 => self.osc_clipboard(text),
            104 => self.osc_reset_color_table(text),
            110 | 111 | 112 | 117 => {
                if text.is_empty() {
                    let resource = usize::try_from(parameter - 100).unwrap_or_default();
                    self.emit(OutputAction::ResetXtermColorResource(resource));
                }
            }
            7 => self.emit(OutputAction::SetCurrentWorkingDirectory(utf16_lossy(text))),
            8 => self.osc_hyperlink(text),
            9 => self.emit(OutputAction::ConEmuAction(utf16_lossy(text))),
            133 => self.emit(OutputAction::FinalTermAction(utf16_lossy(text))),
            633 => self.emit(OutputAction::VsCodeAction(utf16_lossy(text))),
            777 => self.emit(OutputAction::UrxvtAction(utf16_lossy(text))),
            1337 => self.emit(OutputAction::ITerm2Action(utf16_lossy(text))),
            9001 => self.emit(OutputAction::WtAction(utf16_lossy(text))),
            _ => self.emit(OutputAction::UnknownSequence),
        }
        self.clear_last_char();
    }

    fn osc_color_table(&mut self, text: &[u16]) {
        let source = utf16_lossy(text);
        let parts = source.split(';').collect::<Vec<_>>();
        for pair in parts.as_chunks::<2>().0 {
            let Ok(index) = pair[0].parse::<usize>() else {
                continue;
            };
            if pair[1] == "?" {
                self.emit(OutputAction::RequestColorTableEntry(index));
            } else if let Some(color) = parse_xterm_color(pair[1]) {
                self.emit(OutputAction::SetColorTableEntry { index, color });
            }
        }
    }

    fn osc_dynamic_colors(&mut self, parameter: i32, text: &[u16]) {
        let source = utf16_lossy(text);
        let mut resource = usize::try_from(parameter).unwrap_or_default();
        for part in source.split(';') {
            if part == "?" {
                self.emit(OutputAction::RequestXtermColorResource(resource));
            } else if let Some(color) = parse_xterm_color(part) {
                self.emit(OutputAction::SetXtermColorResource { resource, color });
            }
            resource = resource.saturating_add(1);
        }
    }

    fn osc_clipboard(&mut self, text: &[u16]) {
        let Some(delimiter) = text
            .iter()
            .position(|code_unit| *code_unit == u16::from(b';'))
        else {
            return;
        };
        let payload = &text[delimiter + 1..];
        if payload == [u16::from(b'?')] {
            return;
        }
        if let Ok(content) = base64::decode_utf16(payload) {
            self.emit(OutputAction::SetClipboard(content));
        }
    }

    fn osc_reset_color_table(&mut self, text: &[u16]) {
        if text.is_empty() {
            self.emit(OutputAction::ResetColorTable);
            return;
        }
        for part in utf16_lossy(text).split(';') {
            let Ok(index) = part.parse::<usize>() else {
                break;
            };
            self.emit(OutputAction::ResetColorTableEntry(index));
        }
    }

    fn osc_hyperlink(&mut self, text: &[u16]) {
        let source = utf16_lossy(text);
        if source == ";" {
            self.emit(OutputAction::EndHyperlink);
            return;
        }
        let Some(delimiter) = source.find(';') else {
            return;
        };
        let parameters = &source[..delimiter];
        let uri = source[delimiter + 1..]
            .chars()
            .take(MAX_URL_LENGTH)
            .collect::<String>();
        if uri.is_empty() {
            self.emit(OutputAction::EndHyperlink);
            return;
        }
        let custom_id = parameters
            .split(':')
            .find_map(|part| part.strip_prefix("id="))
            .unwrap_or_default()
            .to_owned();
        self.emit(OutputAction::AddHyperlink { uri, custom_id });
    }

    fn action_dcs(&mut self, id: VtId, parameters: &Parameters) -> bool {
        let action = if id_is(id, "q") {
            Some(DcsAction::DefineSixelImage(parameters.clone()))
        } else if id_is(id, "{") {
            Some(DcsAction::DownloadDrcs(parameters.clone()))
        } else if id_is(id, "!u") {
            Some(DcsAction::AssignUserPreferenceCharset(parameters.clone()))
        } else if id_is(id, "!z") {
            Some(DcsAction::DefineMacro(parameters.clone()))
        } else if id_is(id, "$p") {
            Some(DcsAction::RestoreTerminalState(parameters.clone()))
        } else if id_is(id, "$q") {
            Some(DcsAction::RequestSetting)
        } else if id_is(id, "$t") {
            Some(DcsAction::RestorePresentationState(parameters.clone()))
        } else {
            None
        };
        self.clear_last_char();
        if let Some(action) = action {
            self.dispatch.begin_dcs(action)
        } else {
            self.emit(OutputAction::UnknownSequence);
            false
        }
    }
}

impl<D: TermDispatch> StateMachineEngine for OutputStateMachineEngine<D> {
    fn unknown_sequence(&mut self) {
        self.emit(OutputAction::UnknownSequence);
    }

    fn action_execute(&mut self, code_unit: u16) -> bool {
        match code_unit {
            ENQ => self.emit(OutputAction::EnquireAnswerback),
            BEL => self.emit(OutputAction::WarningBell),
            BS => self.emit(OutputAction::CursorBackward(1)),
            TAB => self.emit(OutputAction::ForwardTab(1)),
            CR => self.emit(OutputAction::CarriageReturn),
            LF | FF | VT => self.emit(OutputAction::LineFeed(LineFeedType::DependsOnMode)),
            SI => self.emit(OutputAction::LockingShift(0)),
            SO => self.emit(OutputAction::LockingShift(1)),
            SUB => self.emit(OutputAction::Print(0x2426)),
            DEL => self.emit(OutputAction::Print(DEL)),
            _ => {}
        }
        self.clear_last_char();
        true
    }

    fn action_execute_from_escape(&mut self, code_unit: u16) -> bool {
        self.action_execute(code_unit)
    }

    fn action_print(&mut self, code_unit: u16) -> bool {
        if code_unit >= u16::from(b' ') {
            self.last_printed_char = code_unit;
        }
        self.emit(OutputAction::Print(code_unit));
        true
    }

    fn action_print_string(&mut self, text: &[u16]) -> bool {
        let Some(&last) = text.last() else {
            return true;
        };
        if last >= u16::from(b' ') {
            self.last_printed_char = last;
        }
        self.emit(OutputAction::PrintString(text.to_vec()));
        true
    }

    fn action_pass_through_string(&mut self, _text: &[u16]) -> bool {
        true
    }

    fn action_esc_dispatch(&mut self, id: VtId) -> bool {
        self.action_esc(id);
        true
    }

    fn action_vt52_esc_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.action_vt52(id, parameters);
        true
    }

    fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.action_csi(id, parameters);
        true
    }

    fn action_osc_dispatch(&mut self, parameter: i32, text: &[u16]) -> bool {
        self.action_osc(parameter, text);
        true
    }

    fn action_ss3_dispatch(&mut self, _code_unit: u16, _parameters: &Parameters) -> bool {
        self.emit(OutputAction::UnknownSequence);
        self.clear_last_char();
        true
    }

    fn action_dcs_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.action_dcs(id, parameters)
    }

    fn action_dcs_put(&mut self, code_unit: u16) -> bool {
        self.dispatch.dcs_put(code_unit)
    }
}

fn numeric(parameters: &Parameters, index: usize) -> i32 {
    match parameters.at(index) {
        Some(value) if value > 0 => value,
        _ => 1,
    }
}

fn raw_or(parameters: &Parameters, index: usize, default: i32) -> i32 {
    parameters.at(index).unwrap_or(default)
}

fn for_each_parameter(parameters: &Parameters, mut callback: impl FnMut(i32)) {
    for index in 0..parameters.size() {
        callback(raw_or(parameters, index, 0));
    }
}

fn has_sub_params(parameters: &Parameters) -> bool {
    (0..parameters.size()).any(|index| !parameters.sub_params_for(index).is_empty())
}

fn can_accept_sub_params(id: VtId, parameters: &Parameters) -> bool {
    if id_is(id, "m") {
        return true;
    }
    if id_is(id, "$r") || id_is(id, "$t") {
        return (0..4).all(|index| parameters.sub_params_for(index).is_empty());
    }
    false
}

fn charset_action(id: VtId) -> Option<OutputAction> {
    let command = id_byte(id, 0);
    let charset = id.value() >> 8;
    match command {
        b'%' => Some(OutputAction::DesignateCodingSystem(charset)),
        b'(' => Some(OutputAction::Designate94Charset { slot: 0, charset }),
        b')' => Some(OutputAction::Designate94Charset { slot: 1, charset }),
        b'*' => Some(OutputAction::Designate94Charset { slot: 2, charset }),
        b'+' => Some(OutputAction::Designate94Charset { slot: 3, charset }),
        b'-' => Some(OutputAction::Designate96Charset { slot: 1, charset }),
        b'.' => Some(OutputAction::Designate96Charset { slot: 2, charset }),
        b'/' => Some(OutputAction::Designate96Charset { slot: 3, charset }),
        _ => None,
    }
}

fn is_recognized_advanced_csi(id: VtId) -> bool {
    [
        "$r", "$t", "$u", "$v", "$w", "$x", "$z", "${", "$|", "&u", "'}", "'~", "*x", "*y", "*z",
        ",|", ",~", "=u", "?u", ">u", "<u",
    ]
    .iter()
    .any(|candidate| id_is(id, candidate))
}

fn id_is(id: VtId, text: &str) -> bool {
    id.value() == VtId::from_ascii(text).value()
}

fn id_byte(id: VtId, index: usize) -> u8 {
    (id.value() >> index.saturating_mul(8)).to_le_bytes()[0]
}

fn utf16_lossy(text: &[u16]) -> String {
    String::from_utf16_lossy(text)
}

fn parse_xterm_color(specification: &str) -> Option<u32> {
    let components = if let Some(rgb) = specification.strip_prefix("rgb:") {
        let parts = rgb.split('/').collect::<Vec<_>>();
        if parts.len() != 3 {
            return None;
        }
        [
            parse_scaled_hex(parts[0])?,
            parse_scaled_hex(parts[1])?,
            parse_scaled_hex(parts[2])?,
        ]
    } else if let Some(hex) = specification.strip_prefix('#') {
        if hex.len() % 3 != 0 {
            return None;
        }
        let width = hex.len() / 3;
        if !(1..=4).contains(&width) {
            return None;
        }
        [
            parse_scaled_hex(&hex[..width])?,
            parse_scaled_hex(&hex[width..width * 2])?,
            parse_scaled_hex(&hex[width * 2..])?,
        ]
    } else {
        match specification.to_ascii_lowercase().as_str() {
            "black" => [0, 0, 0],
            "red" => [255, 0, 0],
            "green" => [0, 255, 0],
            "yellow" => [255, 255, 0],
            "blue" => [0, 0, 255],
            "magenta" => [255, 0, 255],
            "cyan" => [0, 255, 255],
            "white" => [255, 255, 255],
            _ => return None,
        }
    };

    Some(
        u32::from(components[0])
            | (u32::from(components[1]) << 8)
            | (u32::from(components[2]) << 16),
    )
}

fn parse_scaled_hex(component: &str) -> Option<u8> {
    if component.is_empty() || component.len() > 4 {
        return None;
    }
    let value = u32::from_str_radix(component, 16).ok()?;
    let bits = u32::try_from(component.len().saturating_mul(4)).ok()?;
    let maximum = (1u32 << bits) - 1;
    let scaled = value.saturating_mul(255) / maximum;
    u8::try_from(scaled).ok()
}

#[cfg(test)]
mod tests {
    use super::{
        DcsAction, DeviceAttributesKind, LineFeedType, OutputAction, OutputStateMachineEngine,
        TermDispatch, parse_xterm_color,
    };
    use crate::state_machine::{Parameters, ParserMode, StateMachine, StateMachineEngine, VtId};

    #[derive(Debug, Default)]
    struct RecordingDispatch {
        actions: Vec<OutputAction>,
        accept_dcs: bool,
    }

    impl TermDispatch for RecordingDispatch {
        fn dispatch(&mut self, action: OutputAction) {
            self.actions.push(action);
        }

        fn begin_dcs(&mut self, action: DcsAction) -> bool {
            self.actions.push(OutputAction::DcsBegin(action));
            self.accept_dcs
        }
    }

    fn machine() -> StateMachine<OutputStateMachineEngine<RecordingDispatch>> {
        StateMachine::new(OutputStateMachineEngine::new(RecordingDispatch::default()))
    }

    fn actions(
        machine: &StateMachine<OutputStateMachineEngine<RecordingDispatch>>,
    ) -> &[OutputAction] {
        &machine.engine().dispatch().actions
    }

    #[test]
    fn output_controls_match_the_cpp_dispatch_contract() {
        let mut engine = OutputStateMachineEngine::new(RecordingDispatch::default());
        for code_unit in [
            0x05, 0x07, 0x08, 0x09, 0x0d, 0x0a, 0x0b, 0x0c, 0x0f, 0x0e, 0x1a, 0x7f,
        ] {
            assert!(engine.action_execute(code_unit));
        }
        assert_eq!(
            engine.dispatch().actions,
            [
                OutputAction::EnquireAnswerback,
                OutputAction::WarningBell,
                OutputAction::CursorBackward(1),
                OutputAction::ForwardTab(1),
                OutputAction::CarriageReturn,
                OutputAction::LineFeed(LineFeedType::DependsOnMode),
                OutputAction::LineFeed(LineFeedType::DependsOnMode),
                OutputAction::LineFeed(LineFeedType::DependsOnMode),
                OutputAction::LockingShift(0),
                OutputAction::LockingShift(1),
                OutputAction::Print(0x2426),
                OutputAction::Print(0x7f),
            ]
        );
    }

    #[test]
    fn parser_and_output_engine_dispatch_cursor_movement_with_vt_defaults() {
        let mut machine = machine();
        machine.process_str("\u{1b}[12A\u{1b}[B\u{1b}[0C\u{1b}[4;9H");
        assert_eq!(
            actions(&machine),
            [
                OutputAction::CursorUp(12),
                OutputAction::CursorDown(1),
                OutputAction::CursorForward(1),
                OutputAction::CursorPosition { line: 4, column: 9 },
            ]
        );
    }

    #[test]
    fn multiple_modes_and_erase_parameters_dispatch_individually() {
        let mut machine = machine();
        machine.process_str("\u{1b}[?5;1;6h\u{1b}[3;2J\u{1b}[0;1K");
        assert_eq!(
            actions(&machine),
            [
                OutputAction::SetMode {
                    private: true,
                    enabled: true,
                    mode: 5
                },
                OutputAction::SetMode {
                    private: true,
                    enabled: true,
                    mode: 1
                },
                OutputAction::SetMode {
                    private: true,
                    enabled: true,
                    mode: 6
                },
                OutputAction::EraseInDisplay(3),
                OutputAction::EraseInDisplay(2),
                OutputAction::EraseInLine(0),
                OutputAction::EraseInLine(1),
            ]
        );
    }

    #[test]
    fn repeat_character_uses_the_last_graphical_character_and_resets_after_dispatch() {
        let mut machine = machine();
        machine.process_str("x\u{1b}[3b");
        assert_eq!(
            actions(&machine),
            [
                OutputAction::PrintString(vec![u16::from(b'x')]),
                OutputAction::PrintString(vec![u16::from(b'x'); 3])
            ]
        );

        machine.process_str("\u{1b}[2b");
        assert_eq!(actions(&machine).len(), 2);
    }

    #[test]
    fn osc_title_clipboard_working_directory_and_hyperlink_are_semantic_actions() {
        let mut machine = machine();
        machine.process_str("\u{1b}]2;hello\u{7}\u{1b}]52;c;Zm9v\u{7}\u{1b}]7;file:///tmp\u{7}\u{1b}]8;id=abc;https://example.test\u{7}\u{1b}]8;;\u{7}");
        assert_eq!(
            actions(&machine),
            [
                OutputAction::SetWindowTitle("hello".to_owned()),
                OutputAction::SetClipboard("foo".to_owned()),
                OutputAction::SetCurrentWorkingDirectory("file:///tmp".to_owned()),
                OutputAction::AddHyperlink {
                    uri: "https://example.test".to_owned(),
                    custom_id: "abc".to_owned(),
                },
                OutputAction::EndHyperlink,
            ]
        );
    }

    #[test]
    fn osc_color_commands_parse_xterm_specs_and_queries() {
        let mut machine = machine();
        machine.process_str(
            "\u{1b}]4;3;rgb:ff/00/80;4;?\u{7}\u{1b}]10;#0f0;?\u{7}\u{1b}]104;3;4\u{7}",
        );
        assert_eq!(
            actions(&machine),
            [
                OutputAction::SetColorTableEntry {
                    index: 3,
                    color: 0x0080_00ff
                },
                OutputAction::RequestColorTableEntry(4),
                OutputAction::SetXtermColorResource {
                    resource: 10,
                    color: 0x0000_ff00
                },
                OutputAction::RequestXtermColorResource(11),
                OutputAction::ResetColorTableEntry(3),
                OutputAction::ResetColorTableEntry(4),
            ]
        );
        assert_eq!(parse_xterm_color("#fff"), Some(0x00ff_ffff));
    }

    #[test]
    fn vt52_dispatch_is_available_without_cpp() {
        let mut machine = machine();
        machine.set_parser_mode(ParserMode::Ansi, false);
        machine.process_str("\u{1b}A\u{1b}H\u{1b}Z");
        assert_eq!(
            actions(&machine),
            [
                OutputAction::CursorUp(1),
                OutputAction::CursorPosition { line: 1, column: 1 },
                OutputAction::DeviceAttributes(DeviceAttributesKind::Vt52),
            ]
        );
    }

    #[test]
    fn dcs_support_is_negotiated_by_the_rust_dispatch_boundary() {
        let dispatch = RecordingDispatch {
            accept_dcs: true,
            ..RecordingDispatch::default()
        };
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));
        machine.process_str("\u{1b}P1;2;3qabc\u{1b}\\");
        let recorded = actions(&machine);
        let OutputAction::DcsBegin(DcsAction::DefineSixelImage(parameters)) = &recorded[0] else {
            panic!("expected SIXEL DCS begin action");
        };
        assert_eq!(parameters.values(), &[Some(1), Some(2), Some(3)]);
        assert!(parameters.sub_params_for(0).is_empty());
        assert!(parameters.sub_params_for(1).is_empty());
        assert!(parameters.sub_params_for(2).is_empty());
        assert_eq!(
            &recorded[1..],
            [
                OutputAction::DcsPut(u16::from(b'a')),
                OutputAction::DcsPut(u16::from(b'b')),
                OutputAction::DcsPut(u16::from(b'c')),
                OutputAction::DcsPut(0x1b),
            ]
        );
    }

    #[test]
    fn unsupported_sequences_report_unknown_without_panicking() {
        let mut engine = OutputStateMachineEngine::new(RecordingDispatch::default());
        assert!(engine.action_csi_dispatch(VtId::from_ascii("~"), &Parameters::default()));
        assert!(engine.action_ss3_dispatch(u16::from(b'A'), &Parameters::default()));
        assert_eq!(
            engine.dispatch().actions,
            [OutputAction::UnknownSequence, OutputAction::UnknownSequence]
        );
    }

    #[test]
    fn subparameters_are_accepted_only_for_the_cpp_compatible_sequences() {
        let mut machine = machine();
        machine.process_str("\u{1b}[1:2A\u{1b}[38:2:1:2:3m");
        let recorded = actions(&machine);
        assert_eq!(recorded.len(), 1);
        let OutputAction::SetGraphicsRendition(parameters) = &recorded[0] else {
            panic!("expected SGR dispatch");
        };
        assert_eq!(parameters.values(), &[Some(38)]);
        assert_eq!(
            parameters.sub_params_for(0),
            &[Some(2), Some(1), Some(2), Some(3)]
        );
    }
}
