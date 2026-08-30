//! Portable `ApiRoutines` semantics that do not require Win32 ownership.
//!
//! The native API stores ordinary input-mode bits on the input buffer while
//! keeping insert, quick-edit and auto-position as console-global extended
//! state. This module also owns deterministic title, console-write and
//! screen-buffer scrolling semantics exercised by Microsoft's host contracts.

use crate::input_buffer::InputBuffer;
use terminal_buffer::geometry::{InclusiveRect, Point};
use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};
use terminal_buffer::text_buffer::TextBuffer;

pub const ENABLE_PROCESSED_INPUT: u32 = 0x0001;
pub const ENABLE_LINE_INPUT: u32 = 0x0002;
pub const ENABLE_ECHO_INPUT: u32 = 0x0004;
pub const ENABLE_WINDOW_INPUT: u32 = 0x0008;
pub const ENABLE_MOUSE_INPUT: u32 = 0x0010;
pub const ENABLE_INSERT_MODE: u32 = 0x0020;
pub const ENABLE_QUICK_EDIT_MODE: u32 = 0x0040;
pub const ENABLE_EXTENDED_FLAGS: u32 = 0x0080;
pub const ENABLE_AUTO_POSITION: u32 = 0x0100;
pub const ENABLE_VIRTUAL_TERMINAL_INPUT: u32 = 0x0200;

const EXTENDED_STATE_MASK: u32 =
    ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_AUTO_POSITION;
const VALID_INPUT_MODE_MASK: u32 = ENABLE_PROCESSED_INPUT
    | ENABLE_LINE_INPUT
    | ENABLE_ECHO_INPUT
    | ENABLE_WINDOW_INPUT
    | ENABLE_MOUSE_INPUT
    | EXTENDED_STATE_MASK
    | ENABLE_VIRTUAL_TERMINAL_INPUT;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputModeStatus {
    Success,
    InvalidArgument,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConsoleInputModeState {
    input_buffer: InputBuffer,
    quick_edit_mode: bool,
    auto_position: bool,
    insert_mode: bool,
    cursor_double_mode: bool,
    cooked_read_insert_mode: Option<bool>,
}

impl ConsoleInputModeState {
    #[must_use]
    pub fn from_mode(mode: u32) -> Self {
        let mut input_buffer = InputBuffer::new();
        input_buffer.set_input_mode(mode & !EXTENDED_STATE_MASK);
        Self {
            input_buffer,
            quick_edit_mode: mode & ENABLE_QUICK_EDIT_MODE != 0,
            auto_position: mode & ENABLE_AUTO_POSITION != 0,
            insert_mode: mode & ENABLE_INSERT_MODE != 0,
            cursor_double_mode: true,
            cooked_read_insert_mode: None,
        }
    }

    pub fn begin_cooked_read(&mut self) {
        self.cooked_read_insert_mode = Some(self.insert_mode);
    }

    #[must_use]
    pub const fn input_mode(&self) -> u32 {
        self.input_buffer.input_mode()
    }

    #[must_use]
    pub const fn quick_edit_mode(&self) -> bool {
        self.quick_edit_mode
    }

    #[must_use]
    pub const fn auto_position(&self) -> bool {
        self.auto_position
    }

    #[must_use]
    pub const fn insert_mode(&self) -> bool {
        self.insert_mode
    }

    #[must_use]
    pub const fn cursor_double_mode(&self) -> bool {
        self.cursor_double_mode
    }

    #[must_use]
    pub const fn cooked_read_insert_mode(&self) -> Option<bool> {
        self.cooked_read_insert_mode
    }

    pub fn set_console_input_mode(&mut self, requested_mode: u32) -> InputModeStatus {
        let can_clear_extended = requested_mode & ENABLE_EXTENDED_FLAGS != 0;
        let new_quick_edit = next_extended_flag(
            self.quick_edit_mode,
            requested_mode,
            ENABLE_QUICK_EDIT_MODE,
            can_clear_extended,
        );
        let new_auto_position = next_extended_flag(
            self.auto_position,
            requested_mode,
            ENABLE_AUTO_POSITION,
            can_clear_extended,
        );
        let new_insert_mode = next_extended_flag(
            self.insert_mode,
            requested_mode,
            ENABLE_INSERT_MODE,
            can_clear_extended,
        );

        if new_insert_mode != self.insert_mode {
            self.cursor_double_mode = false;
        }
        self.quick_edit_mode = new_quick_edit;
        self.auto_position = new_auto_position;
        self.insert_mode = new_insert_mode;
        if let Some(cooked_insert) = self.cooked_read_insert_mode.as_mut() {
            *cooked_insert = new_insert_mode;
        }

        self.input_buffer
            .set_input_mode(requested_mode & !EXTENDED_STATE_MASK);

        if requested_mode & !VALID_INPUT_MODE_MASK != 0
            || requested_mode & ENABLE_ECHO_INPUT != 0 && requested_mode & ENABLE_LINE_INPUT == 0
        {
            InputModeStatus::InvalidArgument
        } else {
            InputModeStatus::Success
        }
    }
}

fn next_extended_flag(
    previous: bool,
    requested_mode: u32,
    flag: u32,
    can_clear_extended: bool,
) -> bool {
    if requested_mode & flag != 0 {
        true
    } else if can_clear_extended {
        false
    } else {
        previous
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TitleRead<T> {
    pub data: Vec<T>,
    pub written: usize,
    pub needed: usize,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ConsoleTitleState {
    title: Vec<u16>,
    original_title: Vec<u16>,
}

impl ConsoleTitleState {
    pub fn set_title(&mut self, title: &str) {
        self.title = title.encode_utf16().collect();
    }

    pub fn set_original_title(&mut self, title: &str) {
        self.original_title = title.encode_utf16().collect();
    }

    #[must_use]
    pub fn title(&self) -> &[u16] {
        &self.title
    }

    #[must_use]
    pub fn original_title(&self) -> &[u16] {
        &self.original_title
    }

    #[must_use]
    pub fn get_console_title_w(&self, capacity: usize) -> TitleRead<u16> {
        read_wide_title(&self.title, capacity)
    }

    #[must_use]
    pub fn get_console_original_title_w(&self, capacity: usize) -> TitleRead<u16> {
        read_wide_title(&self.original_title, capacity)
    }

    #[must_use]
    pub fn get_console_title_a(&self, capacity: usize) -> TitleRead<u8> {
        let encoded = String::from_utf16_lossy(&self.title).into_bytes();
        read_narrow_title(encoded, self.title.len(), capacity)
    }

    /// Reads the original title through the caller's output-codepage adapter.
    ///
    /// The host owns buffering/count semantics while the platform boundary owns
    /// the actual Windows codepage transform, matching conhost's separation of
    /// responsibilities.
    #[must_use]
    pub fn get_console_original_title_a<F>(&self, capacity: usize, encode: F) -> TitleRead<u8>
    where
        F: FnOnce(&[u16]) -> Vec<u8>,
    {
        let encoded = encode(&self.original_title);
        read_narrow_title(encoded, self.original_title.len(), capacity)
    }
}

fn read_wide_title(value: &[u16], capacity: usize) -> TitleRead<u16> {
    let needed = value.len();
    let copied = value.len().min(capacity.saturating_sub(1));
    let mut data = Vec::with_capacity(copied.saturating_add(usize::from(capacity != 0)));
    data.extend_from_slice(&value[..copied]);
    if capacity != 0 {
        data.push(0);
    }
    TitleRead {
        data,
        written: copied,
        needed,
    }
}

fn read_narrow_title(encoded: Vec<u8>, needed: usize, capacity: usize) -> TitleRead<u8> {
    let copied = encoded.len().min(capacity.saturating_sub(1));
    let mut data = Vec::with_capacity(copied.saturating_add(usize::from(capacity != 0)));
    data.extend_from_slice(&encoded[..copied]);
    if capacity != 0 {
        data.push(0);
    }
    TitleRead {
        data,
        written: copied.saturating_add(usize::from(capacity != 0)),
        needed,
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConsoleCodePage {
    Usa437,
    Japanese932,
    Utf8,
}

impl TryFrom<u32> for ConsoleCodePage {
    type Error = ();

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        match value {
            437 => Ok(Self::Usa437),
            932 => Ok(Self::Japanese932),
            65001 => Ok(Self::Utf8),
            _ => Err(()),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConsoleWriteStatus {
    Success,
    Wait,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ConsoleWriteResult {
    pub status: ConsoleWriteStatus,
    pub consumed: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConsoleWriter {
    code_page: ConsoleCodePage,
    wait: bool,
    pending: Vec<u8>,
    output: Vec<u16>,
}

impl ConsoleWriter {
    #[must_use]
    pub fn new(code_page: ConsoleCodePage) -> Self {
        Self {
            code_page,
            wait: false,
            pending: Vec::new(),
            output: Vec::new(),
        }
    }

    pub fn set_wait(&mut self, wait: bool) {
        self.wait = wait;
    }

    pub fn set_code_page(&mut self, code_page: ConsoleCodePage) {
        if self.code_page != code_page {
            self.pending.clear();
            self.code_page = code_page;
        }
    }

    #[must_use]
    pub fn output(&self) -> &[u16] {
        &self.output
    }

    #[must_use]
    pub fn pending_byte_count(&self) -> usize {
        self.pending.len()
    }

    pub fn write_console_w(&mut self, text: &[u16]) -> ConsoleWriteResult {
        if self.wait {
            return ConsoleWriteResult {
                status: ConsoleWriteStatus::Wait,
                consumed: 0,
            };
        }
        self.output.extend_from_slice(text);
        ConsoleWriteResult {
            status: ConsoleWriteStatus::Success,
            consumed: text.len(),
        }
    }

    pub fn write_console_a(&mut self, bytes: &[u8]) -> ConsoleWriteResult {
        if self.wait {
            return ConsoleWriteResult {
                status: ConsoleWriteStatus::Wait,
                consumed: 0,
            };
        }

        match self.code_page {
            ConsoleCodePage::Usa437 => self.decode_cp437(bytes),
            ConsoleCodePage::Japanese932 => self.decode_cp932(bytes),
            ConsoleCodePage::Utf8 => self.decode_utf8(bytes),
        }

        ConsoleWriteResult {
            status: ConsoleWriteStatus::Success,
            consumed: bytes.len(),
        }
    }

    fn decode_cp437(&mut self, bytes: &[u8]) {
        self.pending.clear();
        self.output
            .extend(bytes.iter().map(|byte| u16::from(*byte)));
    }

    fn decode_cp932(&mut self, bytes: &[u8]) {
        let mut combined = core::mem::take(&mut self.pending);
        combined.extend_from_slice(bytes);
        let mut index = 0;
        while index < combined.len() {
            let byte = combined[index];
            if byte <= 0x7f {
                self.output.push(u16::from(byte));
                index += 1;
                continue;
            }
            if (0xa1..=0xdf).contains(&byte) {
                self.output.push(0xff61 + u16::from(byte - 0xa1));
                index += 1;
                continue;
            }
            if is_cp932_lead(byte) {
                if index + 1 == combined.len() {
                    self.pending.push(byte);
                    break;
                }
                let trail = combined[index + 1];
                self.output.push(match (byte, trail) {
                    (0x82, 0xa0) => 0x3042, // あ
                    (0x82, 0xa2) => 0x3044, // い
                    _ => 0xfffd,
                });
                index += 2;
                continue;
            }
            self.output.push(0xfffd);
            index += 1;
        }
    }

    fn decode_utf8(&mut self, bytes: &[u8]) {
        let mut combined = core::mem::take(&mut self.pending);
        combined.extend_from_slice(bytes);
        let mut offset = 0;
        while offset < combined.len() {
            match core::str::from_utf8(&combined[offset..]) {
                Ok(text) => {
                    self.output.extend(text.encode_utf16());
                    return;
                }
                Err(error) => {
                    let valid = error.valid_up_to();
                    if valid != 0 {
                        let prefix = core::str::from_utf8(&combined[offset..offset + valid])
                            .expect("from_utf8 valid_up_to identifies a valid prefix");
                        self.output.extend(prefix.encode_utf16());
                        offset += valid;
                    }
                    if let Some(length) = error.error_len() {
                        self.output.push(0xfffd);
                        offset += length;
                    } else {
                        self.pending.extend_from_slice(&combined[offset..]);
                        return;
                    }
                }
            }
        }
    }
}

const fn is_cp932_lead(byte: u8) -> bool {
    (byte >= 0x81 && byte <= 0x9f) || (byte >= 0xe0 && byte <= 0xfc)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LegacyCell {
    pub character: u16,
    pub attributes: u16,
}

impl LegacyCell {
    #[must_use]
    pub const fn new(character: u16, attributes: u16) -> Self {
        Self {
            character,
            attributes,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ScreenBufferError {
    TextBuffer,
    Row,
    OutOfBounds,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConsoleScreenBuffer {
    buffer: TextBuffer,
    legacy_defaults: LegacyColorDefaults,
    current_attribute: TextAttribute,
    vertical_margins: Option<(u16, u16)>,
}

impl ConsoleScreenBuffer {
    pub fn new(
        width: u16,
        height: u16,
        default_legacy_attribute: u16,
    ) -> Result<Self, ScreenBufferError> {
        let legacy_defaults = LegacyColorDefaults::from_legacy_attribute(default_legacy_attribute);
        let current_attribute =
            TextAttribute::from_legacy(default_legacy_attribute, legacy_defaults);
        let buffer = TextBuffer::new(width, height, current_attribute)
            .map_err(|_| ScreenBufferError::TextBuffer)?;
        Ok(Self {
            buffer,
            legacy_defaults,
            current_attribute,
            vertical_margins: None,
        })
    }

    #[must_use]
    pub const fn width(&self) -> u16 {
        self.buffer.width()
    }

    #[must_use]
    pub const fn height(&self) -> u16 {
        self.buffer.height()
    }

    pub fn set_vertical_margins(&mut self, margins: Option<(u16, u16)>) {
        self.vertical_margins = margins;
    }

    #[must_use]
    pub const fn vertical_margins(&self) -> Option<(u16, u16)> {
        self.vertical_margins
    }

    #[must_use]
    pub fn current_legacy_attribute(&self) -> u16 {
        self.current_attribute
            .legacy_attributes(self.legacy_defaults)
    }

    pub fn set_current_legacy_attribute(&mut self, attributes: u16) {
        self.current_attribute = TextAttribute::from_legacy(attributes, self.legacy_defaults);
    }

    pub fn cell(&self, point: Point) -> Result<LegacyCell, ScreenBufferError> {
        if !self.in_bounds(point) {
            return Err(ScreenBufferError::OutOfBounds);
        }
        let row = self.buffer.row(point.y);
        let character = row
            .glyph_at(point.x)
            .first()
            .copied()
            .unwrap_or(u16::from(b' '));
        let attributes = row
            .attribute_at(point.x)
            .legacy_attributes(self.legacy_defaults);
        Ok(LegacyCell::new(character, attributes))
    }

    pub fn set_cell(&mut self, point: Point, cell: LegacyCell) -> Result<(), ScreenBufferError> {
        if !self.in_bounds(point) {
            return Err(ScreenBufferError::OutOfBounds);
        }
        let attribute = TextAttribute::from_legacy(cell.attributes, self.legacy_defaults);
        let row = self.buffer.row_mut(point.y);
        row.replace_glyph(point.x, 1, &[cell.character])
            .map_err(|_| ScreenBufferError::Row)?;
        row.replace_attributes(point.x, point.x + 1, attribute);
        Ok(())
    }

    pub fn fill_all(&mut self, cell: LegacyCell) -> Result<(), ScreenBufferError> {
        let rect = InclusiveRect::new(
            0,
            0,
            i32::from(self.width()) - 1,
            i32::from(self.height()) - 1,
        );
        self.fill_rect(rect, cell)
    }

    pub fn fill_rect(
        &mut self,
        rect: InclusiveRect,
        cell: LegacyCell,
    ) -> Result<(), ScreenBufferError> {
        let Some(rect) = self.clamp_rect(rect) else {
            return Ok(());
        };
        for y in rect.top..=rect.bottom {
            for x in rect.left..=rect.right {
                self.set_cell(Point::new(x, y), cell)?;
            }
        }
        Ok(())
    }

    /// Implements `ScrollConsoleScreenBufferW` over the safe Rust text store.
    ///
    /// The source is snapshotted before mutation so overlapping moves preserve
    /// their original cells. The clipping rectangle limits writes/fill only;
    /// source cells outside the clip may still move into the clip, matching the
    /// Win32 API. VT scrolling margins are intentionally ignored.
    pub fn scroll_console_screen_buffer(
        &mut self,
        requested_source: InclusiveRect,
        target: Point,
        clip: Option<InclusiveRect>,
        mut fill: LegacyCell,
    ) -> Result<(), ScreenBufferError> {
        let delta = Point::new(
            target.x - requested_source.left,
            target.y - requested_source.top,
        );
        let Some(source) = self.clamp_rect(requested_source) else {
            return Ok(());
        };

        if fill.character == 0 {
            fill.character = u16::from(b' ');
        }
        if fill.attributes == 0 {
            fill.attributes = self.current_legacy_attribute();
        }

        let mut snapshot = Vec::new();
        for y in source.top..=source.bottom {
            for x in source.left..=source.right {
                let point = Point::new(x, y);
                snapshot.push((point, self.cell(point)?));
            }
        }

        let width = i32::from(self.width());
        let height = i32::from(self.height());
        let allowed = |point: Point| {
            point.x >= 0
                && point.y >= 0
                && point.x < width
                && point.y < height
                && clip.is_none_or(|clip| point_in_rect(clip, point))
        };

        // First fill source cells that will not receive moved data. Writes are
        // applied afterwards so overlapping destinations win over fill.
        for (point, _) in &snapshot {
            if !allowed(*point) {
                continue;
            }
            let incoming_source = Point::new(point.x - delta.x, point.y - delta.y);
            if !point_in_rect(source, incoming_source) {
                self.set_cell(*point, fill)?;
            }
        }

        for (point, cell) in snapshot {
            let destination = Point::new(point.x + delta.x, point.y + delta.y);
            if allowed(destination) {
                self.set_cell(destination, cell)?;
            }
        }
        Ok(())
    }

    fn in_bounds(&self, point: Point) -> bool {
        point.x >= 0
            && point.y >= 0
            && point.x < i32::from(self.width())
            && point.y < i32::from(self.height())
    }

    fn clamp_rect(&self, rect: InclusiveRect) -> Option<InclusiveRect> {
        let left = rect.left.max(0);
        let top = rect.top.max(0);
        let right = rect.right.min(i32::from(self.width()) - 1);
        let bottom = rect.bottom.min(i32::from(self.height()) - 1);
        (left <= right && top <= bottom).then(|| InclusiveRect::new(left, top, right, bottom))
    }
}

const fn point_in_rect(rect: InclusiveRect, point: Point) -> bool {
    point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom
}
