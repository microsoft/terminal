//! Safe VT saved-cursor state derived from Host `ScreenBuffer` tests.
//!
//! This owner models DECSC/DECRC state that belongs to the text buffer rather
//! than the parser: cursor position, delayed-wrap state, rendition attributes,
//! selected G0 character set and DECOM origin mode. Margins themselves are not
//! saved; a relative cursor is restored against the margins active at restore
//! time, matching Windows Terminal behavior.

use crate::text_attribute::TextAttribute;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SavedCursorPosition {
    pub x: u16,
    pub y: u16,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum CharacterSet {
    #[default]
    Ascii,
    DecSpecialGraphics,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct CursorSnapshot {
    position: SavedCursorPosition,
    delayed_wrap: bool,
    attributes: TextAttribute,
    charset: CharacterSet,
    origin_mode: bool,
}

impl Default for CursorSnapshot {
    fn default() -> Self {
        Self {
            position: SavedCursorPosition::default(),
            delayed_wrap: false,
            attributes: TextAttribute::default(),
            charset: CharacterSet::Ascii,
            origin_mode: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SavedCursorState {
    width: u16,
    height: u16,
    cursor: SavedCursorPosition,
    delayed_wrap: bool,
    attributes: TextAttribute,
    charset: CharacterSet,
    origin_mode: bool,
    vertical_margins: Option<(u16, u16)>,
    horizontal_margins: Option<(u16, u16)>,
    horizontal_margin_mode: bool,
    saved: CursorSnapshot,
}

impl SavedCursorState {
    #[must_use]
    pub fn new(width: u16, height: u16) -> Self {
        assert!(width > 0);
        assert!(height > 0);
        Self {
            width,
            height,
            cursor: SavedCursorPosition::default(),
            delayed_wrap: false,
            attributes: TextAttribute::default(),
            charset: CharacterSet::Ascii,
            origin_mode: false,
            vertical_margins: None,
            horizontal_margins: None,
            horizontal_margin_mode: false,
            saved: CursorSnapshot::default(),
        }
    }

    #[must_use]
    pub const fn cursor(&self) -> SavedCursorPosition {
        self.cursor
    }

    pub fn set_cursor(&mut self, x: u16, y: u16) {
        self.cursor = SavedCursorPosition {
            x: x.min(self.width - 1),
            y: y.min(self.height - 1),
        };
    }

    #[must_use]
    pub const fn delayed_wrap(&self) -> bool {
        self.delayed_wrap
    }

    pub const fn set_delayed_wrap(&mut self, delayed_wrap: bool) {
        self.delayed_wrap = delayed_wrap;
    }

    #[must_use]
    pub const fn attributes(&self) -> TextAttribute {
        self.attributes
    }

    pub const fn set_attributes(&mut self, attributes: TextAttribute) {
        self.attributes = attributes;
    }

    #[must_use]
    pub const fn charset(&self) -> CharacterSet {
        self.charset
    }

    pub const fn set_charset(&mut self, charset: CharacterSet) {
        self.charset = charset;
    }

    #[must_use]
    pub const fn origin_mode(&self) -> bool {
        self.origin_mode
    }

    pub fn set_vertical_margins(&mut self, top: u16, bottom: u16) {
        assert!(top <= bottom);
        assert!(bottom < self.height);
        self.vertical_margins = Some((top, bottom));
        if self.origin_mode {
            self.home();
        }
    }

    pub fn clear_vertical_margins(&mut self) {
        self.vertical_margins = None;
        if self.origin_mode {
            self.home();
        }
    }

    pub const fn set_horizontal_margin_mode(&mut self, enabled: bool) {
        self.horizontal_margin_mode = enabled;
    }

    pub fn set_horizontal_margins(&mut self, left: u16, right: u16) {
        assert!(left <= right);
        assert!(right < self.width);
        self.horizontal_margins = Some((left, right));
        if self.origin_mode {
            self.home();
        }
    }

    pub fn clear_horizontal_margins(&mut self) {
        self.horizontal_margins = None;
        if self.origin_mode {
            self.home();
        }
    }

    pub fn set_origin_mode(&mut self, enabled: bool) {
        self.origin_mode = enabled;
        self.home();
    }

    pub fn home(&mut self) {
        let (left, top, _, _) = self.active_bounds();
        self.cursor = SavedCursorPosition { x: left, y: top };
        self.delayed_wrap = false;
    }

    pub fn save_cursor(&mut self) {
        let (left, top, _, _) = self.active_bounds();
        let position = if self.origin_mode {
            SavedCursorPosition {
                x: self.cursor.x.saturating_sub(left),
                y: self.cursor.y.saturating_sub(top),
            }
        } else {
            self.cursor
        };
        self.saved = CursorSnapshot {
            position,
            delayed_wrap: self.delayed_wrap,
            attributes: self.attributes,
            charset: self.charset,
            origin_mode: self.origin_mode,
        };
    }

    pub fn restore_cursor(&mut self) {
        self.origin_mode = self.saved.origin_mode;
        let (left, top, right, bottom) = self.active_bounds();
        let (x, y) = if self.saved.origin_mode {
            (
                left.saturating_add(self.saved.position.x).min(right),
                top.saturating_add(self.saved.position.y).min(bottom),
            )
        } else {
            (
                self.saved.position.x.min(self.width - 1),
                self.saved.position.y.min(self.height - 1),
            )
        };
        self.cursor = SavedCursorPosition { x, y };
        self.delayed_wrap = self.saved.delayed_wrap;
        self.attributes = self.saved.attributes;
        self.charset = self.saved.charset;
    }

    /// DECSTR resets both the active saved-cursor state and the slot DECRC will
    /// subsequently restore from.
    pub fn soft_reset(&mut self) {
        self.cursor = SavedCursorPosition::default();
        self.delayed_wrap = false;
        self.attributes = TextAttribute::default();
        self.charset = CharacterSet::Ascii;
        self.origin_mode = false;
        self.saved = CursorSnapshot::default();
    }

    #[must_use]
    pub fn render_with_charset(&self, text: &str) -> String {
        text.chars().map(|ch| self.translate_char(ch)).collect()
    }

    fn active_bounds(&self) -> (u16, u16, u16, u16) {
        if !self.origin_mode {
            return (0, 0, self.width - 1, self.height - 1);
        }
        let (top, bottom) = self.vertical_margins.unwrap_or((0, self.height - 1));
        let (left, right) = if self.horizontal_margin_mode {
            self.horizontal_margins.unwrap_or((0, self.width - 1))
        } else {
            (0, self.width - 1)
        };
        (left, top, right, bottom)
    }

    fn translate_char(&self, ch: char) -> char {
        if self.charset != CharacterSet::DecSpecialGraphics {
            return ch;
        }
        match ch {
            'l' => '┌',
            'w' => '┬',
            'k' => '┐',
            'm' => '└',
            'v' => '┴',
            'j' => '┘',
            _ => ch,
        }
    }
}
