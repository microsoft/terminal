//! Public output-engine compatibility facade.
//!
//! The R01 engine remains the protocol core. R08 adds the XTerm/XParse color
//! normalization Windows Terminal performs before dispatching OSC color
//! commands. R09 can collapse this facade once the migration no longer needs
//! the compatibility seam.

use crate::output_engine_core as core;
use crate::state_machine::{Parameters, StateMachineEngine, VtId};

pub use core::{
    DcsAction, DeviceAttributesKind, LineFeedType, LineRendition, MAX_URL_LENGTH, OutputAction,
    TermDispatch,
};

pub struct OutputStateMachineEngine<D: TermDispatch> {
    inner: core::OutputStateMachineEngine<D>,
}

impl<D: TermDispatch> OutputStateMachineEngine<D> {
    #[must_use]
    pub const fn new(dispatch: D) -> Self {
        Self {
            inner: core::OutputStateMachineEngine::new(dispatch),
        }
    }

    #[must_use]
    pub const fn dispatch(&self) -> &D {
        self.inner.dispatch()
    }

    pub const fn dispatch_mut(&mut self) -> &mut D {
        self.inner.dispatch_mut()
    }

    #[must_use]
    pub fn into_dispatch(self) -> D {
        self.inner.into_dispatch()
    }
}

impl<D: TermDispatch> StateMachineEngine for OutputStateMachineEngine<D> {
    fn unknown_sequence(&mut self) {
        self.inner.unknown_sequence();
    }

    fn encountered_win32_input_mode_sequence(&self) -> bool {
        self.inner.encountered_win32_input_mode_sequence()
    }

    fn action_execute(&mut self, code_unit: u16) -> bool {
        self.inner.action_execute(code_unit)
    }

    fn action_execute_from_escape(&mut self, code_unit: u16) -> bool {
        self.inner.action_execute_from_escape(code_unit)
    }

    fn action_print(&mut self, code_unit: u16) -> bool {
        self.inner.action_print(code_unit)
    }

    fn action_print_string(&mut self, text: &[u16]) -> bool {
        self.inner.action_print_string(text)
    }

    fn action_pass_through_string(&mut self, text: &[u16]) -> bool {
        self.inner.action_pass_through_string(text)
    }

    fn action_esc_dispatch(&mut self, id: VtId) -> bool {
        self.inner.action_esc_dispatch(id)
    }

    fn action_vt52_esc_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.inner.action_vt52_esc_dispatch(id, parameters)
    }

    fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.inner.action_csi_dispatch(id, parameters)
    }

    fn action_osc_dispatch(&mut self, parameter: i32, text: &[u16]) -> bool {
        let Some(normalized) = normalize_color_osc(parameter, text) else {
            return self.inner.action_osc_dispatch(parameter, text);
        };
        self.inner.action_osc_dispatch(parameter, &normalized)
    }

    fn action_ss3_dispatch(&mut self, code_unit: u16, parameters: &Parameters) -> bool {
        self.inner.action_ss3_dispatch(code_unit, parameters)
    }

    fn action_dcs_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.inner.action_dcs_dispatch(id, parameters)
    }

    fn action_dcs_put(&mut self, code_unit: u16) -> bool {
        self.inner.action_dcs_put(code_unit)
    }
}

fn normalize_color_osc(parameter: i32, text: &[u16]) -> Option<Vec<u16>> {
    if parameter != 4 && !(10..=19).contains(&parameter) {
        return None;
    }

    let original = String::from_utf16_lossy(text);
    let mut fields = original.split(';').map(str::to_owned).collect::<Vec<_>>();
    let mut changed = false;

    if parameter == 4 {
        for field in fields.iter_mut().skip(1).step_by(2) {
            changed |= normalize_color_field(field);
        }
    } else {
        for field in &mut fields {
            changed |= normalize_color_field(field);
        }
    }

    changed.then(|| fields.join(";").encode_utf16().collect())
}

fn normalize_color_field(field: &mut String) -> bool {
    let replacement = if let Some(hex) = field.strip_prefix('#') {
        parse_xparse_hash(hex)
            .map(|[red, green, blue]| format!("rgb:{red:02x}/{green:02x}/{blue:02x}"))
    } else {
        xorg_contract_color(field)
            .map(|[red, green, blue]| format!("rgb:{red:02x}/{green:02x}/{blue:02x}"))
    };

    let Some(replacement) = replacement else {
        return false;
    };
    *field = replacement;
    true
}

fn parse_xparse_hash(hex: &str) -> Option<[u8; 3]> {
    if !hex.len().is_multiple_of(3) {
        return None;
    }
    let width = hex.len() / 3;
    if !(1..=4).contains(&width) {
        return None;
    }

    Some([
        parse_xparse_component(&hex[..width])?,
        parse_xparse_component(&hex[width..width * 2])?,
        parse_xparse_component(&hex[width * 2..])?,
    ])
}

fn parse_xparse_component(component: &str) -> Option<u8> {
    let value = u16::from_str_radix(component, 16).ok()?;
    match component.len() {
        1 => u8::try_from(value << 4).ok(),
        2 => u8::try_from(value).ok(),
        3 => u8::try_from(value >> 4).ok(),
        4 => u8::try_from(value >> 8).ok(),
        _ => None,
    }
}

fn xorg_contract_color(name: &str) -> Option<[u8; 3]> {
    let mut stem = String::with_capacity(name.len());
    for character in name.chars() {
        if !character.is_ascii() {
            return None;
        }
        if matches!(character, ' ' | '\u{c}' | '\n' | '\r' | '\t' | '\u{b}') {
            continue;
        }
        stem.push(character.to_ascii_lowercase());
    }

    match stem.as_str() {
        // Values are the base entries in Microsoft's XOrg color tables.
        "darkorange" => Some([255, 140, 0]),
        "orange" => Some([255, 165, 0]),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn xparse_hash_uses_high_bits_instead_of_rgb_component_scaling() {
        assert_eq!(parse_xparse_hash("111"), Some([0x10, 0x10, 0x10]));
        assert_eq!(parse_xparse_hash("222"), Some([0x20, 0x20, 0x20]));
        assert_eq!(parse_xparse_hash("123456"), Some([0x12, 0x34, 0x56]));
        assert_eq!(parse_xparse_hash("123456789"), Some([0x12, 0x45, 0x78]));
        assert_eq!(parse_xparse_hash("123456789abc"), Some([0x12, 0x56, 0x9a]));
        assert_eq!(parse_xparse_hash("1"), None);
    }

    #[test]
    fn microsoft_output_engine_xorg_names_are_ascii_case_insensitive() {
        assert_eq!(xorg_contract_color("DarkOrange"), Some([255, 140, 0]));
        assert_eq!(xorg_contract_color("dark orange"), Some([255, 140, 0]));
        assert_eq!(xorg_contract_color("orange"), Some([255, 165, 0]));
    }
}
