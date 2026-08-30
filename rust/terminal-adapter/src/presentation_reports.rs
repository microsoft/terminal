//! Portable DEC presentation-state report support.
//!
//! Windows Terminal's DECRQPSR reports are deterministic terminal state. Rust
//! owns tabulation-stop reporting/restoration plus DECCIR cursor-information
//! serialization and parsing while the product decorator supplies the live
//! cursor/TextAttribute values that already belong to the Adapter owner.

use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
use terminal_parser::{
    output_engine::{DcsAction, OutputAction, TermDispatch},
    state_machine::{Parameters, VtId},
};

use crate::{adapt_dispatch::Point, vt_response::VtResponseEngine};

const ESC: u16 = 0x1b;
const CURSOR_INFORMATION_REPORT: i32 = 1;
const TABULATION_STOP_REPORT: i32 = 2;
const MAX_RESTORE_PAYLOAD: usize = 4096;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TabulationStopState {
    restored_stops: Option<Vec<i32>>,
    restore_buffer: Option<String>,
}

impl TabulationStopState {
    #[must_use]
    pub fn restoring(&self) -> bool {
        self.restore_buffer.is_some()
    }

    pub fn begin_restore(&mut self, parameters: &Parameters) -> bool {
        if parameters.at(0).unwrap_or(0) != TABULATION_STOP_REPORT {
            return false;
        }
        self.restore_buffer = Some(String::new());
        true
    }

    pub fn put_restore(&mut self, code_unit: u16) -> bool {
        let Some(buffer) = self.restore_buffer.as_mut() else {
            return false;
        };

        if code_unit == ESC {
            let payload = self.restore_buffer.take().unwrap_or_default();
            self.restore(&payload);
            return false;
        }

        let Ok(byte) = u8::try_from(code_unit) else {
            self.restore_buffer = None;
            return false;
        };
        if !byte.is_ascii() || buffer.len() >= MAX_RESTORE_PAYLOAD {
            self.restore_buffer = None;
            return false;
        }

        buffer.push(char::from(byte));
        true
    }

    pub fn clear_all(&mut self) {
        self.restored_stops = Some(Vec::new());
    }

    #[must_use]
    pub fn report(&self, width: i32) -> String {
        let width = width.max(1);
        let stops = self.visible_stops(width);
        let payload = stops
            .iter()
            .map(i32::to_string)
            .collect::<Vec<_>>()
            .join("/");
        format!("\u{1b}P2$u{payload}\u{1b}\\")
    }

    fn restore(&mut self, payload: &str) {
        let mut stops = payload
            .split('/')
            .filter_map(|part| part.parse::<i32>().ok())
            .filter(|stop| *stop > 1)
            .collect::<Vec<_>>();
        stops.sort_unstable();
        stops.dedup();
        self.restored_stops = Some(stops);
    }

    fn visible_stops(&self, width: i32) -> Vec<i32> {
        if let Some(stops) = &self.restored_stops {
            return stops
                .iter()
                .copied()
                .filter(|stop| *stop <= width)
                .collect();
        }

        (9..=width).step_by(8).collect()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct CursorRestore {
    pub(crate) row: i32,
    pub(crate) column: i32,
    pub(crate) page: i32,
    pub(crate) attributes: TextAttribute,
    pub(crate) origin_mode: bool,
    pub(crate) single_shift: Option<u8>,
    pub(crate) delayed_eol_wrap: bool,
    pub(crate) gl: u8,
    pub(crate) gr: u8,
    pub(crate) charset96: [bool; 4],
    pub(crate) charsets: [String; 4],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CursorInformationState {
    active_page: i32,
    single_shift: Option<u8>,
    gl: u8,
    gr: u8,
    charset96: [bool; 4],
    charsets: [String; 4],
    restore_buffer: Option<String>,
    pending_restore: Option<CursorRestore>,
}

impl Default for CursorInformationState {
    fn default() -> Self {
        Self {
            active_page: 1,
            single_shift: None,
            gl: 0,
            gr: 2,
            charset96: [false; 4],
            charsets: std::array::from_fn(|_| String::from("B")),
            restore_buffer: None,
            pending_restore: None,
        }
    }
}

impl CursorInformationState {
    #[must_use]
    pub const fn active_page(&self) -> i32 {
        self.active_page
    }

    #[must_use]
    pub const fn single_shift(&self) -> Option<u8> {
        self.single_shift
    }

    #[must_use]
    pub const fn locking_shifts(&self) -> (u8, u8) {
        (self.gl, self.gr)
    }

    #[must_use]
    pub fn charsets(&self) -> &[String; 4] {
        &self.charsets
    }

    pub fn observe(&mut self, action: &OutputAction) {
        match action {
            OutputAction::SingleShift(slot) => self.single_shift = Some(*slot),
            OutputAction::LockingShift(slot) => self.gl = *slot,
            OutputAction::LockingShiftRight(slot) => self.gr = *slot,
            OutputAction::Designate94Charset { slot, charset } => {
                self.designate(*slot, *charset, false);
            }
            OutputAction::Designate96Charset { slot, charset } => {
                self.designate(*slot, *charset, true);
            }
            OutputAction::PagePositionAbsolute(page) => self.active_page = (*page).max(1),
            OutputAction::PagePositionRelative(count) | OutputAction::NextPage(count) => {
                self.active_page = self.active_page.saturating_add(*count).max(1);
            }
            OutputAction::PagePositionBack(count) | OutputAction::PrecedingPage(count) => {
                self.active_page = self.active_page.saturating_sub(*count).max(1);
            }
            OutputAction::Print(_) | OutputAction::PrintString(_) => self.single_shift = None,
            _ => {}
        }
    }

    #[must_use]
    pub fn report(
        &self,
        cursor: Point,
        viewport_top: i32,
        attributes: TextAttribute,
        origin_mode: bool,
        delayed_eol_wrap: bool,
    ) -> String {
        let row = cursor
            .y
            .saturating_sub(viewport_top)
            .saturating_add(1)
            .max(1);
        let column = cursor.x.saturating_add(1).max(1);
        let rendition = flag_char(rendition_bits(attributes));
        let protected = flag_char(u8::from(attributes.is_protected()));
        let mut flags = u8::from(origin_mode);
        flags |= match self.single_shift {
            Some(2) => 0b0010,
            Some(3) => 0b0100,
            _ => 0,
        };
        flags |= if delayed_eol_wrap { 0b1000 } else { 0 };
        let sizes = self
            .charset96
            .iter()
            .enumerate()
            .fold(0u8, |bits, (index, enabled)| {
                bits | if *enabled { 1u8 << index } else { 0 }
            });
        let charsets = self.charsets.concat();

        format!(
            "\u{1b}P1$u{row};{column};{};{rendition};{protected};{};{};{};{};{charsets}\u{1b}\\",
            self.active_page,
            flag_char(flags),
            self.gl,
            self.gr,
            flag_char(sizes)
        )
    }

    pub fn begin_restore(&mut self, parameters: &Parameters) -> bool {
        if parameters.at(0).unwrap_or(0) != CURSOR_INFORMATION_REPORT {
            return false;
        }
        self.restore_buffer = Some(String::new());
        self.pending_restore = None;
        true
    }

    pub fn put_restore(&mut self, code_unit: u16) -> bool {
        let Some(buffer) = self.restore_buffer.as_mut() else {
            return false;
        };

        if code_unit == ESC {
            let payload = self.restore_buffer.take().unwrap_or_default();
            if let Some(restored) = parse_cursor_restore(&payload) {
                self.apply_protocol_restore(&restored);
                self.pending_restore = Some(restored);
            }
            return false;
        }

        let Ok(byte) = u8::try_from(code_unit) else {
            self.restore_buffer = None;
            return false;
        };
        if !byte.is_ascii() || buffer.len() >= MAX_RESTORE_PAYLOAD {
            self.restore_buffer = None;
            return false;
        }
        buffer.push(char::from(byte));
        true
    }

    fn take_restore(&mut self) -> Option<CursorRestore> {
        self.pending_restore.take()
    }

    fn designate(&mut self, slot: u8, charset: u64, is_96: bool) {
        let index = usize::from(slot);
        if index >= self.charsets.len() {
            return;
        }
        let id = charset_id_from_value(charset);
        if !id.is_empty() {
            self.charsets[index] = id;
            self.charset96[index] = is_96;
        }
    }

    fn apply_protocol_restore(&mut self, restored: &CursorRestore) {
        self.active_page = restored.page.max(1);
        self.single_shift = restored.single_shift;
        self.gl = restored.gl;
        self.gr = restored.gr;
        self.charset96 = restored.charset96;
        self.charsets.clone_from(&restored.charsets);
    }
}

fn rendition_bits(attributes: TextAttribute) -> u8 {
    u8::from(attributes.is_intense())
        | (u8::from(attributes.is_underlined()) << 1)
        | (u8::from(attributes.is_blinking()) << 2)
        | (u8::from(attributes.is_reverse_video()) << 3)
        | (u8::from(attributes.is_invisible()) << 4)
}

fn flag_char(bits: u8) -> char {
    char::from(b'@'.saturating_add(bits & 0x3f))
}

fn parse_flag(text: &str) -> Option<u8> {
    let [byte] = text.as_bytes() else {
        return None;
    };
    (*byte >= b'@').then_some(*byte - b'@')
}

fn charset_id_from_value(mut value: u64) -> String {
    let mut bytes = Vec::new();
    while value != 0 {
        bytes.push((value & 0xff) as u8);
        value >>= 8;
    }
    String::from_utf8(bytes).unwrap_or_default()
}

fn parse_charset_ids(payload: &str) -> Option<[String; 4]> {
    let bytes = payload.as_bytes();
    let mut offset = 0usize;
    let mut ids = Vec::with_capacity(4);
    for _ in 0..4 {
        let start = offset;
        while offset < bytes.len() && (0x20..=0x2f).contains(&bytes[offset]) {
            offset += 1;
        }
        if offset >= bytes.len() || !(0x30..=0x7e).contains(&bytes[offset]) {
            return None;
        }
        offset += 1;
        ids.push(String::from_utf8(bytes[start..offset].to_vec()).ok()?);
    }
    if offset != bytes.len() {
        return None;
    }
    ids.try_into().ok()
}

fn parse_cursor_restore(payload: &str) -> Option<CursorRestore> {
    let fields = payload.split(';').collect::<Vec<_>>();
    if fields.len() != 10 {
        return None;
    }

    let row = fields[0].parse::<i32>().ok()?.max(1);
    let column = fields[1].parse::<i32>().ok()?.max(1);
    let page = fields[2].parse::<i32>().ok()?.max(1);
    let rendition = parse_flag(fields[3])?;
    let protected = parse_flag(fields[4])?;
    let flags = parse_flag(fields[5])?;
    let gl = fields[6].parse::<u8>().ok()?;
    let gr = fields[7].parse::<u8>().ok()?;
    let sizes = parse_flag(fields[8])?;
    let charsets = parse_charset_ids(fields[9])?;

    let mut attributes = TextAttribute::default();
    attributes.set_intense(rendition & 0b00001 != 0);
    if rendition & 0b00010 != 0 {
        attributes.set_underline_style(UnderlineStyle::Single);
    }
    attributes.set_blinking(rendition & 0b00100 != 0);
    attributes.set_reverse_video(rendition & 0b01000 != 0);
    attributes.set_invisible(rendition & 0b10000 != 0);
    attributes.set_protected(protected & 0b1 != 0);

    Some(CursorRestore {
        row,
        column,
        page,
        attributes,
        origin_mode: flags & 0b0001 != 0,
        single_shift: if flags & 0b0100 != 0 {
            Some(3)
        } else if flags & 0b0010 != 0 {
            Some(2)
        } else {
            None
        },
        delayed_eol_wrap: flags & 0b1000 != 0,
        gl,
        gr,
        charset96: std::array::from_fn(|index| sizes & (1u8 << index) != 0),
        charsets,
    })
}

#[derive(Debug, Clone)]
pub struct PresentationReportEngine {
    width: i32,
    tabulation_stops: TabulationStopState,
    cursor_information: CursorInformationState,
    responses: VtResponseEngine,
}

impl PresentationReportEngine {
    #[must_use]
    pub fn new(width: i32) -> Self {
        Self {
            width: width.max(1),
            tabulation_stops: TabulationStopState::default(),
            cursor_information: CursorInformationState::default(),
            responses: VtResponseEngine::default(),
        }
    }

    pub fn set_width(&mut self, width: i32) {
        self.width = width.max(1);
    }

    #[must_use]
    pub const fn tabulation_stops(&self) -> &TabulationStopState {
        &self.tabulation_stops
    }

    #[must_use]
    pub const fn cursor_information(&self) -> &CursorInformationState {
        &self.cursor_information
    }

    pub fn observe(&mut self, action: &OutputAction) {
        self.cursor_information.observe(action);
    }

    #[must_use]
    pub fn response(&self) -> &str {
        self.responses.response()
    }

    pub fn clear_response(&mut self) {
        self.responses.clear();
    }

    pub const fn set_response_writable(&mut self, writable: bool) {
        self.responses.set_writable(writable);
    }

    #[must_use]
    pub fn is_cursor_information_report(action: &OutputAction) -> bool {
        matches!(
            action,
            OutputAction::AdvancedCsi { id, parameters }
                if *id == VtId::from_ascii("$w")
                    && parameters.at(0).unwrap_or(0) == CURSOR_INFORMATION_REPORT
        )
    }

    #[must_use]
    pub fn is_tabulation_report(action: &OutputAction) -> bool {
        matches!(
            action,
            OutputAction::AdvancedCsi { id, parameters }
                if *id == VtId::from_ascii("$w")
                    && parameters.at(0).unwrap_or(0) == TABULATION_STOP_REPORT
        )
    }

    #[must_use]
    pub fn is_clear_all_tabs(action: &OutputAction) -> bool {
        matches!(action, OutputAction::TabClear(3))
    }

    #[must_use]
    pub fn handles_restore(action: &DcsAction) -> bool {
        matches!(
            action,
            DcsAction::RestorePresentationState(parameters)
                if matches!(parameters.at(0).unwrap_or(0), CURSOR_INFORMATION_REPORT | TABULATION_STOP_REPORT)
        )
    }

    pub fn request_cursor_information_report(
        &mut self,
        cursor: Point,
        viewport_top: i32,
        attributes: TextAttribute,
        origin_mode: bool,
        delayed_eol_wrap: bool,
    ) -> bool {
        let response = self.cursor_information.report(
            cursor,
            viewport_top,
            attributes,
            origin_mode,
            delayed_eol_wrap,
        );
        self.responses.return_response(&response)
    }

    pub(crate) fn take_cursor_restore(&mut self) -> Option<CursorRestore> {
        self.cursor_information.take_restore()
    }

    fn request_tabulation_report(&mut self) -> bool {
        let response = self.tabulation_stops.report(self.width);
        self.responses.return_response(&response)
    }
}

impl TermDispatch for PresentationReportEngine {
    fn dispatch(&mut self, action: OutputAction) {
        if Self::is_tabulation_report(&action) {
            let _ = self.request_tabulation_report();
        } else if Self::is_clear_all_tabs(&action) {
            self.tabulation_stops.clear_all();
        } else {
            self.observe(&action);
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        match action {
            DcsAction::RestorePresentationState(parameters) => {
                if parameters.at(0).unwrap_or(0) == CURSOR_INFORMATION_REPORT {
                    self.cursor_information.begin_restore(&parameters)
                } else {
                    self.tabulation_stops.begin_restore(&parameters)
                }
            }
            _ => false,
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        if self.cursor_information.restore_buffer.is_some() {
            self.cursor_information.put_restore(code_unit)
        } else {
            self.tabulation_stops.put_restore(code_unit)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_parser::{output_engine::OutputStateMachineEngine, state_machine::StateMachine};

    fn restore(state: &mut TabulationStopState, payload: &str) {
        assert!(state.begin_restore(&Parameters::from_values(vec![Some(2)])));
        for unit in payload.encode_utf16() {
            assert!(state.put_restore(unit));
        }
        assert!(!state.put_restore(ESC));
    }

    #[test]
    fn microsoft_tabulation_stop_report_matches_default_restore_resize_and_clear_contract() {
        let mut state = TabulationStopState::default();
        assert_eq!(
            state.report(80),
            "\u{1b}P2$u9/17/25/33/41/49/57/65/73\u{1b}\\"
        );
        assert_eq!(
            state.report(132),
            "\u{1b}P2$u9/17/25/33/41/49/57/65/73/81/89/97/105/113/121/129\u{1b}\\"
        );

        restore(&mut state, "30/60/120/240");
        assert_eq!(state.report(80), "\u{1b}P2$u30/60\u{1b}\\");
        assert_eq!(state.report(132), "\u{1b}P2$u30/60/120\u{1b}\\");

        restore(&mut state, "44/22/66");
        assert_eq!(state.report(80), "\u{1b}P2$u22/44/66\u{1b}\\");

        restore(&mut state, "3//7");
        assert_eq!(state.report(80), "\u{1b}P2$u3/7\u{1b}\\");

        restore(&mut state, "0/5/10");
        assert_eq!(state.report(80), "\u{1b}P2$u5/10\u{1b}\\");

        restore(&mut state, "1/8/18");
        assert_eq!(state.report(80), "\u{1b}P2$u8/18\u{1b}\\");

        state.clear_all();
        assert_eq!(state.report(80), "\u{1b}P2$u\u{1b}\\");
    }

    #[test]
    fn microsoft_tabulation_stop_report_runs_through_the_real_vt_parser() {
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(
            PresentationReportEngine::new(80),
        ));

        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u9/17/25/33/41/49/57/65/73\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}P2$t30/60/120/240\u{1b}\\");
        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u30/60\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.engine_mut().dispatch_mut().set_width(132);
        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u30/60/120\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.process_str("\u{1b}[3g\u{1b}[2$w");
        assert_eq!(machine.engine().dispatch().response(), "\u{1b}P2$u\u{1b}\\");
    }

    #[test]
    fn microsoft_cursor_information_protocol_bits_match_source_encoding() {
        let mut state = CursorInformationState::default();
        let mut attributes = TextAttribute::default();
        attributes.set_intense(true);
        attributes.set_underline_style(UnderlineStyle::Single);
        attributes.set_blinking(true);
        attributes.set_reverse_video(true);
        attributes.set_invisible(true);
        attributes.set_protected(true);
        state.observe(&OutputAction::SingleShift(3));
        state.observe(&OutputAction::LockingShift(1));
        state.observe(&OutputAction::LockingShiftRight(3));
        state.observe(&OutputAction::Designate94Charset {
            slot: 0,
            charset: VtId::from_ascii("%5").value(),
        });
        state.observe(&OutputAction::Designate96Charset {
            slot: 1,
            charset: VtId::from_ascii("H").value(),
        });
        state.observe(&OutputAction::Designate96Charset {
            slot: 2,
            charset: VtId::from_ascii("M").value(),
        });
        state.observe(&OutputAction::Designate96Charset {
            slot: 3,
            charset: VtId::from_ascii("B").value(),
        });

        assert_eq!(
            state.report(Point { x: 99, y: 20 }, 20, attributes, true, true),
            "\u{1b}P1$u1;100;1;_;A;M;1;3;N;%5HMB\u{1b}\\"
        );
        state.observe(&OutputAction::Print(u16::from(b'*')));
        assert_eq!(
            state.report(Point { x: 99, y: 20 }, 20, attributes, true, true),
            "\u{1b}P1$u1;100;1;_;A;I;1;3;N;%5HMB\u{1b}\\"
        );
    }

    #[test]
    fn microsoft_cursor_information_restore_decodes_rendition_flags_and_charsets() {
        let mut state = CursorInformationState::default();
        assert!(state.begin_restore(&Parameters::from_values(vec![Some(1)])));
        let payload = "3;4;1;J;A;J;1;3;N;%5HMB";
        for unit in payload.encode_utf16() {
            assert!(state.put_restore(unit));
        }
        assert!(!state.put_restore(ESC));
        let restored = state.take_restore().expect("valid DECCIR restore");
        assert_eq!((restored.row, restored.column, restored.page), (3, 4, 1));
        assert!(restored.attributes.is_underlined());
        assert!(restored.attributes.is_reverse_video());
        assert!(restored.attributes.is_protected());
        assert!(!restored.origin_mode);
        assert_eq!(restored.single_shift, Some(2));
        assert!(restored.delayed_eol_wrap);
        assert_eq!((restored.gl, restored.gr), (1, 3));
        assert_eq!(restored.charset96, [false, true, true, true]);
        assert_eq!(restored.charsets, ["%5", "H", "M", "B"]);
    }

    #[test]
    fn unrelated_presentation_restore_selector_is_not_consumed() {
        let mut state = TabulationStopState::default();
        assert!(!state.begin_restore(&Parameters::from_values(vec![Some(1)])));
        assert!(!state.restoring());
    }
}
