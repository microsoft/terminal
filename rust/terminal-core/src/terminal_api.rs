//! Terminal-owned aggregate API state composed from portable buffer owners.
//!
//! WIP preserved before full equivalence/debt promotion.

use terminal_buffer::alternate_buffer::CursorState;
use terminal_buffer::color_table::ColorTableState;
use terminal_buffer::hyperlink::HyperlinkStore;
use terminal_buffer::text_color::Rgb;

#[derive(Debug, Clone)]
pub struct TerminalApiState {
    colors: ColorTableState,
    cursor: CursorState,
    hyperlinks: HyperlinkStore,
    current_hyperlink: Option<u16>,
    written_text: String,
    taskbar_state: u8,
    taskbar_progress: u8,
    working_directory: String,
}

impl Default for TerminalApiState {
    fn default() -> Self {
        Self {
            colors: ColorTableState::default(),
            cursor: CursorState::default(),
            hyperlinks: HyperlinkStore::new(),
            current_hyperlink: None,
            written_text: String::new(),
            taskbar_state: 0,
            taskbar_progress: 0,
            working_directory: String::new(),
        }
    }
}

impl TerminalApiState {
    #[must_use]
    pub fn colors(&self) -> &ColorTableState {
        &self.colors
    }

    pub fn set_color_table_entry(&mut self, index: usize, color: Rgb) -> bool {
        if index > u8::MAX as usize {
            return false;
        }
        let payload = format!(
            "{index};rgb:{:02x}/{:02x}/{:02x}",
            color.r, color.g, color.b
        );
        self.colors.apply_osc(4, &payload)
    }

    #[must_use]
    pub const fn cursor(&self) -> CursorState {
        self.cursor
    }

    pub const fn set_cursor_blinking(&mut self, blinking: bool) {
        self.cursor.blinking = blinking;
    }

    pub const fn set_cursor_visible(&mut self, visible: bool) {
        self.cursor.visible = visible;
    }

    pub fn open_hyperlink(&mut self, uri: &str, custom_id: Option<&str>) -> u16 {
        let id = self.hyperlinks.add(uri, custom_id);
        self.current_hyperlink = Some(id);
        id
    }

    pub const fn close_hyperlink(&mut self) {
        self.current_hyperlink = None;
    }

    #[must_use]
    pub const fn current_hyperlink_id(&self) -> Option<u16> {
        self.current_hyperlink
    }

    #[must_use]
    pub fn hyperlink_uri(&self, id: u16) -> Option<&str> {
        self.hyperlinks.uri(id)
    }

    pub fn write_utf16(&mut self, text: &[u16]) -> usize {
        let before = self.written_text.chars().count();
        self.written_text.extend(
            char::decode_utf16(text.iter().copied())
                .map(|decoded| decoded.unwrap_or(char::REPLACEMENT_CHARACTER)),
        );
        self.written_text.chars().count() - before
    }

    #[must_use]
    pub fn written_text(&self) -> &str {
        &self.written_text
    }

    #[must_use]
    pub const fn taskbar_state(&self) -> u8 {
        self.taskbar_state
    }

    #[must_use]
    pub const fn taskbar_progress(&self) -> u8 {
        self.taskbar_progress
    }

    pub fn set_taskbar_progress(&mut self, state: Option<u8>, progress: Option<u16>) -> bool {
        let state = state.unwrap_or(0);
        if state > 4 {
            return false;
        }

        match state {
            0 => {
                self.taskbar_state = 0;
                self.taskbar_progress = 0;
            }
            3 => {
                self.taskbar_state = 3;
            }
            _ => {
                self.taskbar_state = state;
                if let Some(progress) = progress {
                    self.taskbar_progress = progress.min(100) as u8;
                } else if self.taskbar_progress == 0 {
                    self.taskbar_progress = 1;
                }
            }
        }
        true
    }

    #[must_use]
    pub fn working_directory(&self) -> &str {
        &self.working_directory
    }

    pub fn set_working_directory(&mut self, payload: &str) -> bool {
        let value = if let Some(inner) = payload
            .strip_prefix('"')
            .and_then(|value| value.strip_suffix('"'))
        {
            (!inner.is_empty() && !inner.contains('"')).then_some(inner)
        } else {
            (!payload.is_empty() && !payload.contains('"')).then_some(payload)
        };
        let Some(value) = value else {
            return false;
        };

        self.working_directory.clear();
        self.working_directory.push_str(value);
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_terminal_api_color_table_entry_contract() {
        let mut terminal = TerminalApiState::default();
        let color = Rgb::new(100, 0, 0);
        assert!(terminal.set_color_table_entry(0, color));
        assert!(terminal.set_color_table_entry(128, color));
        assert!(terminal.set_color_table_entry(255, color));
        assert!(!terminal.set_color_table_entry(512, color));
    }

    #[test]
    fn microsoft_terminal_api_cursor_visibility_via_state_machine_contract() {
        let mut terminal = TerminalApiState::default();
        assert!(terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_blinking(false);
        assert!(!terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_blinking(true);
        assert!(terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_blinking(false);
        assert!(!terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_blinking(true);
        assert!(terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_visible(false);
        assert!(terminal.cursor().blinking);
        assert!(!terminal.cursor().visible);

        terminal.set_cursor_visible(true);
        assert!(terminal.cursor().blinking);
        assert!(terminal.cursor().visible);

        terminal.set_cursor_blinking(false);
        terminal.set_cursor_visible(false);
        assert!(!terminal.cursor().blinking);
        assert!(!terminal.cursor().visible);
    }

    #[test]
    fn microsoft_terminal_api_print_surrogate_pairs_makes_forward_progress() {
        let mut terminal = TerminalApiState::default();
        let text: Vec<u16> = "𐐌𐐜𐐬".repeat(100).encode_utf16().collect();
        assert_eq!(terminal.write_utf16(&text), 300);
        assert_eq!(terminal.written_text(), "𐐌𐐜𐐬".repeat(100));
    }

    #[test]
    fn microsoft_terminal_api_add_hyperlink_contract() {
        let mut terminal = TerminalApiState::default();
        let id = terminal.open_hyperlink("test.url", None);
        assert_eq!(terminal.current_hyperlink_id(), Some(id));
        assert_eq!(terminal.hyperlink_uri(id), Some("test.url"));

        terminal.write_utf16(&"Hello World".encode_utf16().collect::<Vec<_>>());
        assert_eq!(terminal.current_hyperlink_id(), Some(id));
        assert_eq!(terminal.hyperlink_uri(id), Some("test.url"));

        terminal.close_hyperlink();
        assert_eq!(terminal.current_hyperlink_id(), None);
    }

    #[test]
    fn microsoft_terminal_api_add_hyperlink_custom_id_contract() {
        let mut terminal = TerminalApiState::default();
        let first = terminal.open_hyperlink("test.url", Some("myId"));
        assert_eq!(terminal.current_hyperlink_id(), Some(first));
        assert_eq!(terminal.hyperlink_uri(first), Some("test.url"));

        terminal.write_utf16(&"Hello World".encode_utf16().collect::<Vec<_>>());
        let same = terminal.open_hyperlink("test.url", Some("myId"));
        assert_eq!(first, same);
        assert_eq!(terminal.current_hyperlink_id(), Some(first));
        assert_eq!(terminal.hyperlink_uri(first), Some("test.url"));

        terminal.close_hyperlink();
        assert_eq!(terminal.current_hyperlink_id(), None);
    }

    #[test]
    fn microsoft_terminal_api_add_hyperlink_custom_id_different_uri_contract() {
        let mut terminal = TerminalApiState::default();
        let first = terminal.open_hyperlink("test.url", Some("myId"));
        let second = terminal.open_hyperlink("other.url", Some("myId"));

        assert_ne!(first, second);
        assert_eq!(terminal.current_hyperlink_id(), Some(second));
        assert_eq!(terminal.hyperlink_uri(first), Some("test.url"));
        assert_eq!(terminal.hyperlink_uri(second), Some("other.url"));
    }

    #[test]
    fn microsoft_terminal_api_taskbar_progress_contract() {
        let mut terminal = TerminalApiState::default();
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (0, 0)
        );

        assert!(terminal.set_taskbar_progress(Some(1), Some(50)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (1, 50)
        );

        assert!(terminal.set_taskbar_progress(Some(0), Some(0)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (0, 0)
        );

        assert!(!terminal.set_taskbar_progress(Some(5), Some(50)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (0, 0)
        );

        assert!(terminal.set_taskbar_progress(Some(1), Some(999)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (1, 100)
        );

        assert!(terminal.set_taskbar_progress(None, None));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (0, 0)
        );

        assert!(terminal.set_taskbar_progress(Some(1), Some(80)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (1, 80)
        );

        assert!(terminal.set_taskbar_progress(Some(2), None));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (2, 80)
        );

        assert!(terminal.set_taskbar_progress(Some(3), Some(75)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (3, 80)
        );

        assert!(terminal.set_taskbar_progress(Some(0), Some(50)));
        assert_eq!(
            (terminal.taskbar_state(), terminal.taskbar_progress()),
            (0, 0)
        );

        assert!(terminal.set_taskbar_progress(Some(2), None));
        assert_eq!(terminal.taskbar_state(), 2);
        assert!(terminal.taskbar_progress() > 0);
    }

    #[test]
    fn microsoft_terminal_api_working_directory_contract() {
        let mut terminal = TerminalApiState::default();
        assert_eq!(terminal.working_directory(), "");

        assert!(!terminal.set_working_directory(""));
        assert_eq!(terminal.working_directory(), "");
        assert!(!terminal.set_working_directory("\""));
        assert_eq!(terminal.working_directory(), "");
        assert!(!terminal.set_working_directory("No quotes \"until\" later"));
        assert_eq!(terminal.working_directory(), "");

        assert!(terminal.set_working_directory("\"C:\\\""));
        assert_eq!(terminal.working_directory(), "C:\\");
        assert!(terminal.set_working_directory("\"C:\\Program Files\""));
        assert_eq!(terminal.working_directory(), "C:\\Program Files");
        assert!(terminal.set_working_directory("\"D:\\中文\""));
        assert_eq!(terminal.working_directory(), "D:\\中文");

        assert!(terminal.set_working_directory("C:\\"));
        assert_eq!(terminal.working_directory(), "C:\\");
        assert!(terminal.set_working_directory("C:\\Program Files"));
        assert_eq!(terminal.working_directory(), "C:\\Program Files");
        assert!(terminal.set_working_directory("D:\\中文"));
        assert_eq!(terminal.working_directory(), "D:\\中文");
    }
}
