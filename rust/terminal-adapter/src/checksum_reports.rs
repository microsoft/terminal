//! Portable DECRQCRA checksum-report owner.
//!
//! Windows Terminal's optional checksum report is deliberately deterministic:
//! it walks the requested text cells, subtracts their UTF-16 code units, the
//! DEC-era rendition bits, and legacy color indexes, then returns a 16-bit
//! checksum in a DCS response. This module owns that calculation against a safe
//! Rust [`TextBuffer`] so the adapter response path no longer needs the C++
//! `TextBuffer` implementation to reproduce Microsoft's contract.

use terminal_buffer::{
    text_attribute::TextAttribute, text_buffer::TextBuffer, text_color::TextColor,
};
use terminal_parser::{
    output_engine::{OutputAction, TermDispatch},
    state_machine::{Parameters, VtId},
};

use crate::{
    adapt_dispatch::PageGeometry, decrqss_color_alias::ColorAliasIndices,
    presentation_state::AdaptDispatchPresentationState, vt_response::VtResponseEngine,
};

const CHECKSUM_REPORT_ID: &str = "*y";
const SYMBOL_FOR_SUBSTITUTE: u16 = 0x2426;
const ESCAPE: u16 = 0x1b;
const MAX_ROW_WIDTH: i32 = 0x7fff;

#[derive(Debug, Clone)]
pub struct ChecksumReportEngine {
    presentation: AdaptDispatchPresentationState,
    buffer: Option<TextBuffer>,
    cursor_x: u16,
    cursor_y: u16,
    enabled: bool,
    aliases: ColorAliasIndices,
    responses: VtResponseEngine,
}

impl ChecksumReportEngine {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        let width = u16::try_from(geometry.width.clamp(1, MAX_ROW_WIDTH)).unwrap_or(1);
        let height = u16::try_from(geometry.height.clamp(1, i32::from(u16::MAX))).unwrap_or(1);
        Self {
            presentation: AdaptDispatchPresentationState::new(geometry),
            buffer: TextBuffer::new(width, height, TextAttribute::default()).ok(),
            cursor_x: 0,
            cursor_y: 0,
            enabled: false,
            aliases: ColorAliasIndices::default(),
            responses: VtResponseEngine::default(),
        }
    }

    #[must_use]
    pub const fn presentation(&self) -> &AdaptDispatchPresentationState {
        &self.presentation
    }

    pub const fn presentation_mut(&mut self) -> &mut AdaptDispatchPresentationState {
        &mut self.presentation
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

    pub const fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    pub const fn set_color_alias_indices(&mut self, aliases: ColorAliasIndices) {
        self.aliases = aliases;
    }

    #[must_use]
    pub fn handles(id: VtId) -> bool {
        id == VtId::from_ascii(CHECKSUM_REPORT_ID)
    }

    /// Records printable UTF-16 units in the report buffer using the attributes
    /// owned by the live presentation state. Ordinary display mutation remains
    /// independently visible to the downstream product owner; this buffer is
    /// specifically the state required by the optional DECRQCRA observer.
    pub fn write_text(&mut self, text: &[u16], attributes: TextAttribute) {
        let Some(buffer) = self.buffer.as_mut() else {
            return;
        };
        let width = buffer.width();
        let height = buffer.height();

        for unit in text {
            if self.cursor_x >= width {
                self.cursor_x = 0;
                self.cursor_y = self.cursor_y.saturating_add(1).min(height - 1);
            }

            let row = buffer.row_mut(i32::from(self.cursor_y));
            if row
                .replace_glyph(i32::from(self.cursor_x), 1, &[*unit])
                .is_ok()
            {
                row.replace_attributes(
                    i32::from(self.cursor_x),
                    i32::from(self.cursor_x.saturating_add(1)),
                    attributes,
                );
            }
            self.cursor_x = self.cursor_x.saturating_add(1);
        }
    }

    /// Emits a DCS checksum response for a parsed DECRQCRA request.
    ///
    /// Parameters are `id;page;top;left;bottom;right`, using DEC's 1-based
    /// inclusive rectangle coordinates. Disabled optional checksum support, page
    /// zero, and pages not represented by this product buffer intentionally
    /// report a zero checksum, matching the Microsoft implementation's fallback.
    pub fn request(&mut self, parameters: &Parameters) -> bool {
        let id = parameters.at(0).unwrap_or(0);
        let checksum = self.checksum(parameters);
        self.responses
            .return_response(&format!("\u{1b}P{id}!~{checksum:04X}\u{1b}\\"))
    }

    fn checksum(&self, parameters: &Parameters) -> u16 {
        let page = parameters.at(1).unwrap_or(1);
        let Some(buffer) = self.buffer.as_ref() else {
            return 0;
        };
        if !self.enabled || page != 1 {
            return 0;
        }

        let width = i32::from(buffer.width());
        let height = i32::from(buffer.height());
        let top = parameters.at(2).unwrap_or(1).max(1).min(height) - 1;
        let left = parameters.at(3).unwrap_or(1).max(1).min(width) - 1;
        let bottom = parameters.at(4).unwrap_or(height).max(1).min(height) - 1;
        let right = parameters.at(5).unwrap_or(width).max(1).min(width) - 1;
        if bottom < top || right < left {
            return 0;
        }

        let default_foreground = if self.aliases.default_foreground < 16 {
            self.aliases.default_foreground
        } else {
            7
        };
        let default_background = if self.aliases.default_background < 16 {
            self.aliases.default_background
        } else {
            0
        };

        let mut checksum = 0u16;
        for y in top..=bottom {
            let row = buffer.row(y);
            for x in left..=right {
                for unit in row.glyph_at(x) {
                    checksum = checksum.wrapping_sub(if *unit == SYMBOL_FOR_SUBSTITUTE {
                        ESCAPE
                    } else {
                        *unit
                    });
                }

                let attribute = row.attribute_at(x);
                checksum = checksum.wrapping_sub(if attribute.is_protected() { 0x04 } else { 0 });
                checksum = checksum.wrapping_sub(if attribute.is_invisible() { 0x08 } else { 0 });
                checksum = checksum.wrapping_sub(if attribute.is_underlined() { 0x10 } else { 0 });
                checksum = checksum.wrapping_sub(if attribute.is_reverse_video() {
                    0x20
                } else {
                    0
                });
                checksum = checksum.wrapping_sub(if attribute.is_blinking() { 0x40 } else { 0 });
                checksum = checksum.wrapping_sub(if attribute.is_intense() { 0x80 } else { 0 });

                let foreground = Self::color_index(attribute.foreground(), default_foreground);
                let background = Self::color_index(attribute.background(), default_background);
                checksum = checksum.wrapping_sub(foreground << 4);
                checksum = checksum.wrapping_sub(background);
            }
        }
        checksum
    }

    fn color_index(color: TextColor, default_index: usize) -> u16 {
        if color.is_legacy() {
            u16::from(color.index())
        } else {
            u16::try_from(default_index).unwrap_or_default()
        }
    }
}

impl TermDispatch for ChecksumReportEngine {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::Print(unit) => {
                let attributes = self.presentation.current_attributes();
                self.write_text(&[unit], attributes);
                self.presentation.dispatch(OutputAction::Print(unit));
            }
            OutputAction::PrintString(text) => {
                let attributes = self.presentation.current_attributes();
                self.write_text(&text, attributes);
                self.presentation.dispatch(OutputAction::PrintString(text));
            }
            OutputAction::AdvancedCsi { id, parameters } if Self::handles(id) => {
                if !self.request(&parameters) {
                    self.presentation
                        .dispatch(OutputAction::AdvancedCsi { id, parameters });
                }
            }
            other => self.presentation.dispatch(other),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::{text_attribute::UnderlineStyle, text_color::TextColor};
    use terminal_parser::{output_engine::OutputStateMachineEngine, state_machine::StateMachine};

    fn report(text: &str, attributes: TextAttribute) -> String {
        let mut dispatch = ChecksumReportEngine::new(PageGeometry::new(0, 100, 29));
        dispatch.set_enabled(true);
        dispatch
            .presentation_mut()
            .set_current_attributes(attributes);
        let encoded_len = text.encode_utf16().count();
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));
        machine.process_str(text);
        machine.process_str(&format!("\u{1b}[99;1;1;1;1;{encoded_len}*y"));
        machine.engine().dispatch().response().to_owned()
    }

    #[test]
    fn microsoft_decrqcra_ascii_and_latin1_vectors_match() {
        for (text, checksum) in [
            ("A", "FF4F"),
            (" ", "FF70"),
            ("~", "FF12"),
            ("ABC", "FDEA"),
            ("Á", "FECF"),
            ("¡", "FEEF"),
            ("ÿ", "FE91"),
            ("ÁÂÃ", "FC6A"),
        ] {
            assert_eq!(
                report(text, TextAttribute::default()),
                format!("\u{1b}P99!~{checksum}\u{1b}\\"),
                "text={text:?}"
            );
        }
    }

    #[test]
    fn microsoft_decrqcra_rendition_and_protection_vectors_match() {
        let mut intense = TextAttribute::default();
        intense.set_intense(true);
        assert_eq!(report("A", intense), "\u{1b}P99!~FECF\u{1b}\\");

        let mut underline = TextAttribute::default();
        underline.set_underline_style(UnderlineStyle::Single);
        assert_eq!(report("A", underline), "\u{1b}P99!~FF3F\u{1b}\\");

        let mut blinking = TextAttribute::default();
        blinking.set_blinking(true);
        assert_eq!(report("A", blinking), "\u{1b}P99!~FF0F\u{1b}\\");

        let mut reverse = TextAttribute::default();
        reverse.set_reverse_video(true);
        assert_eq!(report("A", reverse), "\u{1b}P99!~FF2F\u{1b}\\");

        let mut invisible = TextAttribute::default();
        invisible.set_invisible(true);
        assert_eq!(report("A", invisible), "\u{1b}P99!~FF47\u{1b}\\");

        let mut combined = TextAttribute::default();
        combined.set_intense(true);
        combined.set_underline_style(UnderlineStyle::Single);
        combined.set_reverse_video(true);
        assert_eq!(report("A", combined), "\u{1b}P99!~FE9F\u{1b}\\");

        let mut protected = TextAttribute::default();
        protected.set_protected(true);
        assert_eq!(report("A", protected), "\u{1b}P99!~FF4B\u{1b}\\");
        assert_eq!(report("B", protected), "\u{1b}P99!~FF4A\u{1b}\\");
    }

    #[test]
    fn microsoft_decrqcra_color_vectors_match() {
        let mut foreground = TextAttribute::default();
        foreground.set_foreground(TextColor::index16(TextColor::DARK_RED));
        assert_eq!(report("A", foreground), "\u{1b}P99!~FFAF\u{1b}\\");

        let mut background = TextAttribute::default();
        background.set_background(TextColor::index16(TextColor::DARK_GREEN));
        assert_eq!(report("A", background), "\u{1b}P99!~FF4D\u{1b}\\");

        let mut combined = TextAttribute::default();
        combined.set_foreground(TextColor::index16(TextColor::DARK_YELLOW));
        combined.set_background(TextColor::index16(TextColor::DARK_BLUE));
        assert_eq!(report("A", combined), "\u{1b}P99!~FF8B\u{1b}\\");
    }

    #[test]
    fn disabled_or_unrepresented_pages_report_zero_and_sink_failure_is_atomic() {
        let mut engine = ChecksumReportEngine::new(PageGeometry::new(0, 80, 24));
        let request =
            Parameters::from_values(vec![Some(7), Some(1), Some(1), Some(1), Some(1), Some(1)]);
        assert!(engine.request(&request));
        assert_eq!(engine.response(), "\u{1b}P7!~0000\u{1b}\\");

        engine.clear_response();
        engine.set_enabled(true);
        let page_zero =
            Parameters::from_values(vec![Some(8), Some(0), Some(1), Some(1), Some(1), Some(1)]);
        assert!(engine.request(&page_zero));
        assert_eq!(engine.response(), "\u{1b}P8!~0000\u{1b}\\");

        engine.clear_response();
        engine.set_response_writable(false);
        engine.dispatch(OutputAction::AdvancedCsi {
            id: VtId::from_ascii(CHECKSUM_REPORT_ID),
            parameters: request,
        });
        assert!(engine.response().is_empty());
        assert_eq!(engine.presentation().core().deferred_actions().len(), 1);
    }
}
