//! Portable DEC screen-alignment (DECALN) state used by Host `ScreenBufferTests`.
//!
//! DECALN is deliberately modeled at the screen-buffer seam: it fills only the
//! active viewport with `E`, leaves scrollback/out-of-view rows untouched,
//! homes the cursor, clears margins, and removes rendition/meta attributes from
//! the active erase state.

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct AlignmentAttributes {
    pub foreground: u8,
    pub background: u8,
    pub bold: bool,
    pub underline: bool,
    pub reverse: bool,
    pub protected: bool,
}

impl AlignmentAttributes {
    #[must_use]
    pub const fn standard_erase(self) -> Self {
        Self {
            foreground: self.foreground,
            background: self.background,
            bold: false,
            underline: false,
            reverse: false,
            protected: false,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AlignmentCell {
    pub ch: char,
    pub attributes: AlignmentAttributes,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ScreenAlignmentState {
    width: usize,
    height: usize,
    viewport_top: usize,
    viewport_height: usize,
    cells: Vec<AlignmentCell>,
    cursor: (usize, usize),
    vertical_margins: Option<(usize, usize)>,
    horizontal_margins: Option<(usize, usize)>,
    active_attributes: AlignmentAttributes,
}

impl ScreenAlignmentState {
    #[must_use]
    pub fn new(width: usize, height: usize, viewport_top: usize, viewport_height: usize) -> Self {
        assert!(width > 0 && height > 0);
        assert!(viewport_height > 0 && viewport_top + viewport_height <= height);
        Self {
            width,
            height,
            viewport_top,
            viewport_height,
            cells: vec![
                AlignmentCell {
                    ch: ' ',
                    attributes: AlignmentAttributes::default()
                };
                width * height
            ],
            cursor: (0, viewport_top),
            vertical_margins: None,
            horizontal_margins: None,
            active_attributes: AlignmentAttributes::default(),
        }
    }

    pub fn fill_all(&mut self, ch: char, attributes: AlignmentAttributes) {
        self.cells.fill(AlignmentCell { ch, attributes });
    }

    pub fn set_cursor(&mut self, x: usize, y: usize) {
        assert!(x < self.width && y < self.height);
        self.cursor = (x, y);
    }

    pub fn set_margins(
        &mut self,
        vertical: Option<(usize, usize)>,
        horizontal: Option<(usize, usize)>,
    ) {
        self.vertical_margins = vertical;
        self.horizontal_margins = horizontal;
    }

    pub fn set_active_attributes(&mut self, attributes: AlignmentAttributes) {
        self.active_attributes = attributes;
    }

    /// Executes DECALN (`ESC # 8`).
    pub fn screen_alignment_pattern(&mut self) {
        let default = AlignmentAttributes::default();
        let start = self.viewport_top * self.width;
        let end = (self.viewport_top + self.viewport_height) * self.width;
        self.cells[start..end].fill(AlignmentCell {
            ch: 'E',
            attributes: default,
        });
        self.cursor = (0, self.viewport_top);
        self.vertical_margins = None;
        self.horizontal_margins = None;
        self.active_attributes = self.active_attributes.standard_erase();
    }

    #[must_use]
    pub fn cell(&self, x: usize, y: usize) -> AlignmentCell {
        self.cells[y * self.width + x]
    }

    #[must_use]
    pub const fn cursor(&self) -> (usize, usize) {
        self.cursor
    }

    #[must_use]
    pub const fn vertical_margins(&self) -> Option<(usize, usize)> {
        self.vertical_margins
    }

    #[must_use]
    pub const fn horizontal_margins(&self) -> Option<(usize, usize)> {
        self.horizontal_margins
    }

    #[must_use]
    pub const fn active_attributes(&self) -> AlignmentAttributes {
        self.active_attributes
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_screen_alignment_pattern_contract() {
        let mut state = ScreenAlignmentState::new(10, 8, 2, 4);
        let buffer_attr = AlignmentAttributes {
            foreground: 2,
            background: 4,
            bold: true,
            underline: true,
            reverse: false,
            protected: false,
        };
        state.fill_all('Z', buffer_attr);

        let initial_attr = AlignmentAttributes {
            foreground: 5,
            background: 1,
            bold: true,
            underline: true,
            reverse: true,
            protected: true,
        };
        state.set_active_attributes(initial_attr);
        state.set_margins(Some((3, 4)), Some((2, 7)));
        state.set_cursor(5, 4);

        state.screen_alignment_pattern();

        for y in 2..6 {
            for x in 0..10 {
                assert_eq!(
                    state.cell(x, y),
                    AlignmentCell {
                        ch: 'E',
                        attributes: AlignmentAttributes::default()
                    }
                );
            }
        }
        for y in [0, 1, 6, 7] {
            for x in 0..10 {
                assert_eq!(
                    state.cell(x, y),
                    AlignmentCell {
                        ch: 'Z',
                        attributes: buffer_attr
                    }
                );
            }
        }
        assert_eq!(state.cursor(), (0, 2));
        assert_eq!(state.vertical_margins(), None);
        assert_eq!(state.horizontal_margins(), None);
        assert_eq!(state.active_attributes(), initial_attr.standard_erase());
    }
}
