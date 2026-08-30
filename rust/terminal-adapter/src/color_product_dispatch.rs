//! Product owner for adapter color-table mutation and VT color reports.
//!
//! The parser already emits typed color actions and `terminal-buffer` owns the
//! mutable color table. This decorator connects those two existing owners and
//! serializes the report families that Microsoft's Adapter tests observe.

use terminal_buffer::{
    color_table::ColorTableState,
    text_color::{
        CURSOR_COLOR, DEFAULT_BACKGROUND, DEFAULT_FOREGROUND, Rgb, SELECTION_BACKGROUND, TABLE_SIZE,
    },
};
use terminal_parser::{
    output_engine::{DcsAction, OutputAction, TermDispatch},
    state_machine::{Parameters, VtId},
};

use crate::{
    adapt_dispatch::PageGeometry, decrqss_color_alias::ColorAliasIndices,
    product_dispatch::AdaptDispatchProductState,
};

const PALETTE_SIZE: usize = 256;
const ESC: &str = "\u{1b}";
const ST: &str = "\u{1b}\\";

/// Final product decorator for color-table mutation and color-report responses.
///
/// `ColorTableState` remains the canonical palette owner. The small override
/// arrays only model renderer slots that are intentionally outside OSC 4's
/// 0..255 palette (default/cursor/selection colors) and the Microsoft
/// `INVALID_COLOR` sentinel used by the Xterm resource report contract.
pub struct ColorProductDispatch {
    inner: AdaptDispatchProductState,
    colors: ColorTableState,
    overrides: [Option<Rgb>; TABLE_SIZE],
    representable: [bool; TABLE_SIZE],
    aliases: ColorAliasIndices,
    outbound: String,
    writable: bool,
}

impl ColorProductDispatch {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        let aliases = ColorAliasIndices::default();
        let mut inner = AdaptDispatchProductState::new(geometry);
        inner.set_color_alias_indices(aliases);
        Self {
            inner,
            colors: ColorTableState::default(),
            overrides: [None; TABLE_SIZE],
            representable: [true; TABLE_SIZE],
            aliases,
            outbound: String::new(),
            writable: true,
        }
    }

    #[must_use]
    pub const fn inner(&self) -> &AdaptDispatchProductState {
        &self.inner
    }

    pub const fn inner_mut(&mut self) -> &mut AdaptDispatchProductState {
        &mut self.inner
    }

    #[must_use]
    pub const fn aliases(&self) -> ColorAliasIndices {
        self.aliases
    }

    pub fn set_color_alias_indices(&mut self, aliases: ColorAliasIndices) {
        self.aliases = aliases;
        self.inner.set_color_alias_indices(aliases);
    }

    pub fn set_response_writable(&mut self, writable: bool) {
        self.writable = writable;
        self.inner.set_response_writable(writable);
    }

    #[must_use]
    pub fn response(&self) -> &str {
        &self.outbound
    }

    pub fn clear_response(&mut self) {
        self.outbound.clear();
        self.inner.clear_response();
    }

    /// Renderer-equivalent setup seam for any Windows Terminal color-table slot.
    /// Palette slots 0..255 mutate the canonical `ColorTableState`; special
    /// slots are represented as narrow overrides because OSC 4 cannot address
    /// them.
    pub fn set_color(&mut self, index: usize, color: Rgb) -> bool {
        if index >= TABLE_SIZE {
            return false;
        }

        self.representable[index] = true;
        if index < PALETTE_SIZE {
            self.overrides[index] = None;
            self.colors.apply_osc(
                4,
                &format!(
                    "{index};rgb:{:02x}/{:02x}/{:02x}",
                    color.r, color.g, color.b
                ),
            )
        } else {
            self.overrides[index] = Some(color);
            true
        }
    }

    /// Models the renderer's `INVALID_COLOR` sentinel without inventing a fake
    /// RGB value. Unrepresentable resources intentionally produce no response.
    pub fn set_color_representable(&mut self, index: usize, representable: bool) -> bool {
        let Some(slot) = self.representable.get_mut(index) else {
            return false;
        };
        *slot = representable;
        true
    }

    #[must_use]
    pub fn color(&self, index: usize) -> Option<Rgb> {
        if index >= TABLE_SIZE || !self.representable[index] {
            return None;
        }
        self.overrides[index].or_else(|| self.colors.color(index))
    }

    fn set_packed_color(&mut self, index: usize, color: u32) -> bool {
        let rgb = Rgb::new(
            color.to_le_bytes()[0],
            color.to_le_bytes()[1],
            color.to_le_bytes()[2],
        );
        self.set_color(index, rgb)
    }

    fn resource_index(&self, resource: usize) -> Option<usize> {
        match resource {
            10 => Some(self.aliases.default_foreground),
            11 => Some(self.aliases.default_background),
            12 => Some(CURSOR_COLOR),
            17 => Some(SELECTION_BACKGROUND),
            _ => None,
        }
    }

    fn set_xterm_resource(&mut self, resource: usize, color: u32) -> bool {
        let target = match resource {
            10 => DEFAULT_FOREGROUND,
            11 => DEFAULT_BACKGROUND,
            12 => CURSOR_COLOR,
            17 => SELECTION_BACKGROUND,
            _ => return false,
        };
        self.set_packed_color(target, color)
    }

    fn request_palette_entry(&mut self, index: usize) -> bool {
        if index >= PALETTE_SIZE || !self.writable {
            return false;
        }
        let Some(color) = self.color(index) else {
            return true;
        };
        self.outbound
            .push_str(&format!("{ESC}]4;{index};{}{}", xterm_rgb(color), ST));
        true
    }

    fn request_xterm_resource(&mut self, resource: usize) -> bool {
        if !self.writable {
            return false;
        }
        let Some(index) = self.resource_index(resource) else {
            // Microsoft treats unsupported resources as handled but silent.
            return true;
        };
        let Some(color) = self.color(index) else {
            // `INVALID_COLOR` is also a handled, silent request.
            return true;
        };
        self.outbound
            .push_str(&format!("{ESC}]{resource};{}{}", xterm_rgb(color), ST));
        true
    }

    fn request_color_table_report(&mut self, parameters: &Parameters) -> bool {
        if parameters.at(0).unwrap_or_default() != 2 {
            return false;
        }
        let model = parameters.at(1).unwrap_or_default();
        if !matches!(model, 1 | 2) || !self.writable {
            return false;
        }

        let mut response = format!("{ESC}P2$s");
        for index in 0..TABLE_SIZE {
            let Some(color) = self.color(index) else {
                return true;
            };
            if index != 0 {
                response.push('/');
            }
            if model == 1 {
                let (hue, lightness, saturation) = rgb_to_dec_hls(color);
                response.push_str(&format!("{index};1;{hue};{lightness};{saturation}"));
            } else {
                response.push_str(&format!(
                    "{index};2;{};{};{}",
                    byte_to_percent(color.r),
                    byte_to_percent(color.g),
                    byte_to_percent(color.b)
                ));
            }
        }
        response.push_str(ST);
        self.outbound.push_str(&response);
        true
    }

    fn collect_inner_response(&mut self) {
        if self.inner.response().is_empty() {
            return;
        }
        self.outbound.push_str(self.inner.response());
        self.inner.clear_response();
    }
}

impl TermDispatch for ColorProductDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::SetColorTableEntry { index, color } if index < PALETTE_SIZE => {
                let _ = self.set_packed_color(index, color);
            }
            action @ OutputAction::RequestColorTableEntry(index) => {
                if !self.request_palette_entry(index) {
                    self.inner.dispatch(action);
                    self.collect_inner_response();
                }
            }
            OutputAction::SetXtermColorResource { resource, color } => {
                if !self.set_xterm_resource(resource, color) {
                    self.inner
                        .dispatch(OutputAction::SetXtermColorResource { resource, color });
                    self.collect_inner_response();
                }
            }
            action @ OutputAction::RequestXtermColorResource(resource) => {
                if !self.request_xterm_resource(resource) {
                    self.inner.dispatch(action);
                    self.collect_inner_response();
                }
            }
            OutputAction::AdvancedCsi { id, parameters } if id == VtId::from_ascii("$u") => {
                if !self.request_color_table_report(&parameters) {
                    self.inner
                        .dispatch(OutputAction::AdvancedCsi { id, parameters });
                    self.collect_inner_response();
                }
            }
            other => {
                self.inner.dispatch(other);
                self.collect_inner_response();
            }
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.inner.begin_dcs(action)
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        let handled = self.inner.dcs_put(code_unit);
        self.collect_inner_response();
        handled
    }
}

fn xterm_rgb(color: Rgb) -> String {
    format!(
        "rgb:{:04x}/{:04x}/{:04x}",
        u16::from(color.r) * 0x0101,
        u16::from(color.g) * 0x0101,
        u16::from(color.b) * 0x0101
    )
}

fn byte_to_percent(value: u8) -> u32 {
    (u32::from(value) * 100 + 127) / 255
}

fn rgb_to_dec_hls(color: Rgb) -> (u32, u32, u32) {
    let red = f64::from(color.r) / 255.0;
    let green = f64::from(color.g) / 255.0;
    let blue = f64::from(color.b) / 255.0;
    let maximum = red.max(green).max(blue);
    let minimum = red.min(green).min(blue);
    let delta = maximum - minimum;
    let lightness = f64::midpoint(maximum, minimum);

    if delta == 0.0 {
        return (0, (lightness * 100.0).round() as u32, 0);
    }

    let saturation = delta / (1.0 - (2.0 * lightness - 1.0).abs());
    let standard_hue = if maximum == red {
        60.0 * ((green - blue) / delta).rem_euclid(6.0)
    } else if maximum == green {
        60.0 * (((blue - red) / delta) + 2.0)
    } else {
        60.0 * (((red - green) / delta) + 4.0)
    };
    let dec_hue = (standard_hue.round() as i32 + 120).rem_euclid(360) as u32;
    (
        dec_hue,
        (lightness * 100.0).round() as u32,
        (saturation * 100.0).round() as u32,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    const VT525: [Rgb; 16] = [
        Rgb::new(0, 0, 0),
        Rgb::new(204, 36, 36),
        Rgb::new(51, 204, 51),
        Rgb::new(204, 204, 51),
        Rgb::new(51, 51, 204),
        Rgb::new(204, 51, 204),
        Rgb::new(51, 204, 204),
        Rgb::new(120, 120, 120),
        Rgb::new(69, 69, 69),
        Rgb::new(255, 0, 0),
        Rgb::new(0, 255, 0),
        Rgb::new(255, 255, 0),
        Rgb::new(0, 0, 255),
        Rgb::new(255, 0, 255),
        Rgb::new(0, 255, 255),
        Rgb::new(255, 255, 255),
    ];

    fn product() -> ColorProductDispatch {
        ColorProductDispatch::new(PageGeometry::new(20, 100, 29))
    }

    fn load_vt525(state: &mut ColorProductDispatch, black_tail: bool) {
        for (index, color) in VT525.into_iter().enumerate() {
            assert!(state.set_color(index, color));
        }
        if black_tail {
            for index in 16..TABLE_SIZE {
                assert!(state.set_color(index, Rgb::new(0, 0, 0)));
            }
        }
    }

    fn expected_hls_report() -> String {
        let first = [
            "0;1;0;0;0",
            "1;1;120;47;70",
            "2;1;240;50;60",
            "3;1;180;50;60",
            "4;1;0;50;60",
            "5;1;60;50;60",
            "6;1;300;50;60",
            "7;1;0;47;0",
            "8;1;0;27;0",
            "9;1;120;50;100",
            "10;1;240;50;100",
            "11;1;180;50;100",
            "12;1;0;50;100",
            "13;1;60;50;100",
            "14;1;300;50;100",
            "15;1;0;100;0",
        ];
        let mut response = format!("{ESC}P2$s{}", first.join("/"));
        for index in 16..TABLE_SIZE {
            response.push_str(&format!("/{index};1;0;0;0"));
        }
        response.push_str(ST);
        response
    }

    fn expected_rgb_report() -> String {
        let first = [
            "0;2;0;0;0",
            "1;2;80;14;14",
            "2;2;20;80;20",
            "3;2;80;80;20",
            "4;2;20;20;80",
            "5;2;80;20;80",
            "6;2;20;80;80",
            "7;2;47;47;47",
            "8;2;27;27;27",
            "9;2;100;0;0",
            "10;2;0;100;0",
            "11;2;100;100;0",
            "12;2;0;0;100",
            "13;2;100;0;100",
            "14;2;0;100;100",
            "15;2;100;100;100",
        ];
        let mut response = format!("{ESC}P2$s{}", first.join("/"));
        for index in 16..TABLE_SIZE {
            response.push_str(&format!("/{index};2;0;0;0"));
        }
        response.push_str(ST);
        response
    }

    #[test]
    fn microsoft_color_table_report_tests_match_hls_and_rgb_full_table() {
        for (model, expected) in [(1, expected_hls_report()), (2, expected_rgb_report())] {
            let mut state = product();
            load_vt525(&mut state, true);
            state.dispatch(OutputAction::AdvancedCsi {
                id: VtId::from_ascii("$u"),
                parameters: Parameters::from_values(vec![Some(2), Some(model)]),
            });
            assert_eq!(state.response(), expected);
        }
    }

    #[test]
    fn microsoft_osc4_color_palette_report_tests_match_all_source_entries() {
        let expected = [
            "rgb:0000/0000/0000",
            "rgb:cccc/2424/2424",
            "rgb:3333/cccc/3333",
            "rgb:cccc/cccc/3333",
            "rgb:3333/3333/cccc",
            "rgb:cccc/3333/cccc",
            "rgb:3333/cccc/cccc",
            "rgb:7878/7878/7878",
            "rgb:4545/4545/4545",
            "rgb:ffff/0000/0000",
            "rgb:0000/ffff/0000",
            "rgb:ffff/ffff/0000",
            "rgb:0000/0000/ffff",
            "rgb:ffff/0000/ffff",
            "rgb:0000/ffff/ffff",
            "rgb:ffff/ffff/ffff",
        ];

        let mut state = product();
        load_vt525(&mut state, true);
        for (index, color) in expected.into_iter().enumerate() {
            state.dispatch(OutputAction::RequestColorTableEntry(index));
            assert_eq!(state.response(), format!("{ESC}]4;{index};{color}{ST}"));
            state.clear_response();
        }
    }

    #[test]
    fn microsoft_xterm_color_resource_report_tests_match_alias_and_cursor_cases() {
        let mut state = product();
        load_vt525(&mut state, false);
        assert!(state.set_color(DEFAULT_FOREGROUND, Rgb::new(190, 190, 190)));
        assert!(state.set_color(DEFAULT_BACKGROUND, Rgb::new(12, 12, 12)));
        assert!(state.set_color(CURSOR_COLOR, Rgb::new(255, 0, 0)));

        state.dispatch(OutputAction::RequestXtermColorResource(10));
        assert_eq!(state.response(), format!("{ESC}]10;rgb:7878/7878/7878{ST}"));
        state.clear_response();

        let mut aliases = state.aliases();
        aliases.default_foreground = DEFAULT_FOREGROUND;
        state.set_color_alias_indices(aliases);
        state.dispatch(OutputAction::RequestXtermColorResource(10));
        assert_eq!(state.response(), format!("{ESC}]10;rgb:bebe/bebe/bebe{ST}"));
        state.clear_response();

        state.dispatch(OutputAction::RequestXtermColorResource(11));
        assert_eq!(state.response(), format!("{ESC}]11;rgb:0000/0000/0000{ST}"));
        state.clear_response();

        aliases.default_background = DEFAULT_BACKGROUND;
        state.set_color_alias_indices(aliases);
        state.dispatch(OutputAction::RequestXtermColorResource(11));
        assert_eq!(state.response(), format!("{ESC}]11;rgb:0c0c/0c0c/0c0c{ST}"));
        state.clear_response();

        aliases.default_foreground = 1;
        aliases.default_background = 10;
        state.set_color_alias_indices(aliases);
        state.dispatch(OutputAction::RequestXtermColorResource(10));
        state.dispatch(OutputAction::RequestXtermColorResource(11));
        assert_eq!(
            state.response(),
            format!("{ESC}]10;rgb:cccc/2424/2424{ST}{ESC}]11;rgb:0000/ffff/0000{ST}")
        );
        state.clear_response();

        state.dispatch(OutputAction::RequestXtermColorResource(12));
        assert_eq!(state.response(), format!("{ESC}]12;rgb:ffff/0000/0000{ST}"));
        state.clear_response();

        assert!(state.set_color_representable(CURSOR_COLOR, false));
        state.dispatch(OutputAction::RequestXtermColorResource(12));
        assert!(state.response().is_empty());

        state.dispatch(OutputAction::RequestXtermColorResource(13));
        assert!(state.response().is_empty());
    }

    #[test]
    fn microsoft_set_color_table_value_matches_full_256_index_domain() {
        let mut state = product();
        let packed_color = 1_u32 | (2_u32 << 8) | (3_u32 << 16);
        for index in 0..PALETTE_SIZE {
            state.dispatch(OutputAction::SetColorTableEntry {
                index,
                color: packed_color,
            });
            assert_eq!(state.color(index), Some(Rgb::new(1, 2, 3)), "index={index}");
        }
    }
}
