//! Portable VT response serialization for adapter reports.
//!
//! This owner intentionally has no host, Win32, renderer, or terminal-input dependency. It
//! serializes deterministic responses and retains the response stream in the same order that
//! `ITerminalApi::ReturnResponse` observes them. Adapter live wiring is a separate integration
//! concern.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VtResponseEngine {
    response: String,
    writable: bool,
}

impl Default for VtResponseEngine {
    fn default() -> Self {
        Self {
            response: String::new(),
            writable: true,
        }
    }
}

impl VtResponseEngine {
    #[must_use]
    pub fn response(&self) -> &str {
        &self.response
    }

    pub fn clear(&mut self) {
        self.response.clear();
    }

    /// Controls whether the response sink accepts writes. This models the
    /// success/failure result of the native `ITerminalApi::ReturnResponse`
    /// boundary without importing that platform interface into portable Rust.
    pub const fn set_writable(&mut self, writable: bool) {
        self.writable = writable;
    }

    /// Writes an already-serialized VT response through the same fallible sink
    /// used by the typed response helpers. This is used by protocol serializers
    /// such as DECRQSS that own their complete DCS framing separately.
    #[must_use]
    pub fn return_response(&mut self, response: &str) -> bool {
        self.push(response)
    }

    #[must_use]
    pub fn operating_status(&mut self) -> bool {
        self.push("\u{1b}[0n")
    }

    #[must_use]
    pub fn cursor_position_report(
        &mut self,
        cursor_x: i32,
        cursor_y: i32,
        viewport_top: i32,
    ) -> bool {
        let row = cursor_y.saturating_sub(viewport_top).saturating_add(1);
        let column = cursor_x.saturating_add(1);
        self.push(&format!("\u{1b}[{row};{column}R"))
    }

    #[must_use]
    pub fn extended_cursor_position_report(
        &mut self,
        cursor_x: i32,
        cursor_y: i32,
        viewport_top: i32,
        page: i32,
    ) -> bool {
        let row = cursor_y.saturating_sub(viewport_top).saturating_add(1);
        let column = cursor_x.saturating_add(1);
        let page = page.max(1);
        self.push(&format!("\u{1b}[?{row};{column};{page}R"))
    }

    #[must_use]
    pub fn primary_device_attributes(&mut self, clipboard_supported: bool) -> bool {
        const BASE_ATTRIBUTES: &str = "\u{1b}[?61;4;6;7;14;21;22;23;24;28;32;42";
        let mut response = String::from(BASE_ATTRIBUTES);
        if clipboard_supported {
            response.push_str(";52");
        }
        response.push('c');
        self.push(&response)
    }

    #[must_use]
    pub fn secondary_device_attributes(&mut self) -> bool {
        self.push("\u{1b}[>0;10;1c")
    }

    #[must_use]
    pub fn tertiary_device_attributes(&mut self) -> bool {
        self.push("\u{1b}P!|00000000\u{1b}\\")
    }

    #[must_use]
    pub fn terminal_parameters(&mut self, reporting_permission: i32) -> bool {
        let response_permission = match reporting_permission {
            0 => 2,
            1 => 3,
            _ => return false,
        };
        self.push(&format!("\u{1b}[{response_permission};1;1;128;128;1;0x"))
    }

    #[must_use]
    pub fn displayed_extent(
        &mut self,
        height: i32,
        width: i32,
        viewport_left: i32,
        page: i32,
    ) -> bool {
        let height = height.max(1);
        let width = width.max(1);
        let left = viewport_left.max(0).saturating_add(1);
        let page = page.max(1);
        self.push(&format!("\u{1b}[{height};{width};{left};1;{page}\"w"))
    }

    /// Serializes a DECRQCRA checksum response. Checksum computation itself is
    /// deliberately owned by the text-buffer layer; the adapter response owner
    /// is responsible only for the exact DCS framing observed by Microsoft.
    #[must_use]
    pub fn checksum_report(&mut self, request_id: u16, checksum: u16) -> bool {
        self.push(&format!("\u{1b}P{request_id}!~{checksum:04X}\u{1b}\\"))
    }

    /// Serializes a DECRQM/DECRPM mode report for a mode owned by the Rust
    /// adapter. DEC reports `1` for set and `2` for reset. Recognition is
    /// deliberately decided by the caller so unsupported modes remain at the
    /// external boundary rather than being reported as implemented.
    #[must_use]
    pub fn mode_report(&mut self, private: bool, mode: i32, enabled: bool) -> bool {
        self.mode_report_state(private, mode, if enabled { 1 } else { 2 })
    }

    /// Serializes a DECRPM response using one of the four DEC mode-status
    /// values: set (`1`), reset (`2`), permanently set (`3`), or permanently
    /// reset (`4`). Values outside that domain are rejected without writing.
    #[must_use]
    pub fn mode_report_state(&mut self, private: bool, mode: i32, state: i32) -> bool {
        if !(1..=4).contains(&state) {
            return false;
        }
        let private_prefix = if private { "?" } else { "" };
        self.push(&format!("\u{1b}[{private_prefix}{mode};{state}$y"))
    }

    fn push(&mut self, response: &str) -> bool {
        if !self.writable {
            return false;
        }
        self.response.push_str(response);
        true
    }
}

#[cfg(test)]
mod tests {
    use super::VtResponseEngine;

    #[test]
    fn microsoft_operating_status_serializes_good_condition() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.operating_status());
        assert_eq!(responses.response(), "\u{1b}[0n");
    }

    #[test]
    fn microsoft_cpr_is_viewport_relative_one_based_and_appends() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.cursor_position_report(50, 34, 20));
        assert_eq!(responses.response(), "\u{1b}[15;51R");
        assert!(responses.cursor_position_report(51, 35, 20));
        assert_eq!(responses.response(), "\u{1b}[15;51R\u{1b}[16;52R");
    }

    #[test]
    fn microsoft_decxcpr_includes_current_page() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.extended_cursor_position_report(50, 34, 20, 1));
        assert_eq!(responses.response(), "\u{1b}[?15;51;1R");
        responses.clear();
        assert!(responses.extended_cursor_position_report(50, 34, 20, 3));
        assert_eq!(responses.response(), "\u{1b}[?15;51;3R");
    }

    #[test]
    fn microsoft_primary_device_attributes_tracks_clipboard_feature() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.primary_device_attributes(true));
        assert_eq!(
            responses.response(),
            "\u{1b}[?61;4;6;7;14;21;22;23;24;28;32;42;52c"
        );
        responses.clear();
        assert!(responses.primary_device_attributes(false));
        assert_eq!(
            responses.response(),
            "\u{1b}[?61;4;6;7;14;21;22;23;24;28;32;42c"
        );
    }

    #[test]
    fn microsoft_secondary_and_tertiary_device_attributes_are_exact() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.secondary_device_attributes());
        assert_eq!(responses.response(), "\u{1b}[>0;10;1c");
        responses.clear();
        assert!(responses.tertiary_device_attributes());
        assert_eq!(responses.response(), "\u{1b}P!|00000000\u{1b}\\");
    }

    #[test]
    fn microsoft_terminal_parameters_serialize_both_reporting_permissions() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.terminal_parameters(0));
        assert_eq!(responses.response(), "\u{1b}[2;1;1;128;128;1;0x");
        responses.clear();
        assert!(responses.terminal_parameters(1));
        assert_eq!(responses.response(), "\u{1b}[3;1;1;128;128;1;0x");
        responses.clear();
        assert!(!responses.terminal_parameters(2));
        assert!(responses.response().is_empty());
    }

    #[test]
    fn microsoft_displayed_extent_serializes_viewport_geometry_and_page() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.displayed_extent(24, 80, 0, 1));
        assert_eq!(responses.response(), "\u{1b}[24;80;1;1;1\"w");
        responses.clear();
        assert!(responses.displayed_extent(24, 80, 5, 3));
        assert_eq!(responses.response(), "\u{1b}[24;80;6;1;3\"w");
    }

    #[test]
    fn microsoft_checksum_report_uses_exact_decrqcra_dcs_framing() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.checksum_report(99, 0xFF4F));
        assert_eq!(responses.response(), "\u{1b}P99!~FF4F\u{1b}\\");
        responses.clear();
        assert!(responses.checksum_report(99, 0xFDEA));
        assert_eq!(responses.response(), "\u{1b}P99!~FDEA\u{1b}\\");
    }

    #[test]
    fn microsoft_decrqm_serializes_standard_private_and_permanent_mode_states() {
        let mut responses = VtResponseEngine::default();
        assert!(responses.mode_report(false, 4, true));
        assert_eq!(responses.response(), "\u{1b}[4;1$y");
        responses.clear();
        assert!(responses.mode_report(false, 4, false));
        assert_eq!(responses.response(), "\u{1b}[4;2$y");
        responses.clear();
        assert!(responses.mode_report(true, 25, true));
        assert_eq!(responses.response(), "\u{1b}[?25;1$y");
        responses.clear();
        assert!(responses.mode_report_state(true, 2027, 3));
        assert_eq!(responses.response(), "\u{1b}[?2027;3$y");
    }

    #[test]
    fn invalid_mode_report_state_is_rejected_without_output() {
        let mut responses = VtResponseEngine::default();
        assert!(!responses.mode_report_state(true, 2027, 0));
        assert!(!responses.mode_report_state(true, 2027, 5));
        assert!(responses.response().is_empty());
    }

    #[test]
    fn rejected_response_write_is_reported_without_mutating_stream() {
        let mut responses = VtResponseEngine::default();
        responses.set_writable(false);
        assert!(!responses.return_response("\u{1b}P1$r0m\u{1b}\\"));
        assert!(!responses.primary_device_attributes(true));
        assert!(!responses.secondary_device_attributes());
        assert!(!responses.tertiary_device_attributes());
        assert!(!responses.terminal_parameters(0));
        assert!(!responses.displayed_extent(24, 80, 0, 1));
        assert!(!responses.checksum_report(99, 0xFF4F));
        assert!(!responses.mode_report(true, 25, true));
        assert!(!responses.mode_report_state(true, 2027, 3));
        assert!(responses.response().is_empty());
    }
}
