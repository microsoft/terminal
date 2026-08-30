//! KeyChord-aware `SettingsModel` projection layered on the portable `ActionMap` owner.
//!
//! `LayeredActionMap` remains the action/keybinding owner. This module adds the
//! `KeyChord` identity required by Microsoft's scan-code contracts without
//! claiming Windows keyboard-layout translation as portable Rust behavior.

use std::collections::BTreeMap;

use crate::{
    keybindings::LayeredActionMap,
    settings_json::{self, JsonValue},
};

pub const MOD_CONTROL: u8 = 0x01;
pub const MOD_ALT: u8 = 0x02;
pub const MOD_SHIFT: u8 = 0x04;
pub const MOD_WIN: u8 = 0x08;

const VK_RETURN: i32 = 0x0D;
const VK_UP: i32 = 0x26;
const VK_DOWN: i32 = 0x28;
const VK_OEM_PLUS: i32 = 0xBB;
const VK_OEM_3: i32 = 0xC0;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum KeyBindingsModelError {
    InvalidJson,
    ExpectedArray,
    ExpectedEntryObject,
    ExpectedKeys,
    ExpectedKeyString,
    InvalidKeyChord,
    Layering,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
enum KeyCodeIdentity {
    Vkey(i32),
    ScanCode(i32),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
struct KeyIdentity {
    modifiers: u8,
    code: KeyCodeIdentity,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KeyChord {
    modifiers: u8,
    vkey: i32,
    scan_code: i32,
}

impl KeyChord {
    #[must_use]
    pub fn new(modifiers: u8, vkey: i32, scan_code: i32) -> Self {
        let effective_vkey = if vkey == 0 && scan_code == 41 {
            // Microsoft uses MapVirtualKeyW here. Scan code 41 is the exact
            // US-layout vector exercised by LayerScancodeKeybindings.
            VK_OEM_3
        } else {
            vkey
        };
        Self {
            modifiers,
            vkey: effective_vkey,
            scan_code,
        }
    }

    /// Parses the deterministic `KeyChord` subset exercised by `KeyBindingsTests`.
    /// Active-layout Windows translation remains an explicit platform boundary.
    ///
    /// # Errors
    ///
    /// Returns [`KeyBindingsModelError::InvalidKeyChord`] for malformed chords.
    pub fn from_string(input: &str) -> Result<Self, KeyBindingsModelError> {
        parse_key(input).map(|(chord, _)| chord)
    }

    #[must_use]
    pub fn modifiers(self) -> u8 {
        self.modifiers
    }

    #[must_use]
    pub fn vkey(self) -> i32 {
        self.vkey
    }

    #[must_use]
    pub fn scan_code(self) -> i32 {
        self.scan_code
    }

    #[must_use]
    pub fn to_binding_string(self) -> String {
        let mut result = String::new();
        append_modifiers(self.modifiers, &mut result);

        if self.scan_code != 0 {
            result.push_str("sc(");
            result.push_str(&self.scan_code.to_string());
            result.push(')');
            return result;
        }

        if (i32::from(b'0')..=i32::from(b'9')).contains(&self.vkey)
            || (i32::from(b'A')..=i32::from(b'Z')).contains(&self.vkey)
        {
            result.push((self.vkey as u8 as char).to_ascii_lowercase());
        } else if let Some(name) = vkey_name(self.vkey) {
            result.push_str(name);
        } else if self.vkey == VK_OEM_3 {
            result.push('`');
        } else if self.vkey != 0 {
            result.push_str("vk(");
            result.push_str(&self.vkey.to_string());
            result.push(')');
        }
        result
    }

    fn identity(self) -> KeyIdentity {
        KeyIdentity {
            modifiers: self.modifiers,
            code: if self.vkey != 0 {
                KeyCodeIdentity::Vkey(self.vkey)
            } else {
                KeyCodeIdentity::ScanCode(self.scan_code)
            },
        }
    }
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct KeyBindingsModel {
    actions: LayeredActionMap,
    effective_keys: BTreeMap<KeyIdentity, String>,
}

impl KeyBindingsModel {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Layers one `ActionMap` vector while tracking Microsoft's effective
    /// `KeyChord` identity separately from its source spelling.
    ///
    /// # Errors
    ///
    /// Returns [`KeyBindingsModelError`] for malformed JSON/chords or if the
    /// underlying portable `ActionMap` rejects the layer.
    pub fn layer_json(&mut self, input: &str) -> Result<(), KeyBindingsModelError> {
        let root = settings_json::parse(input).map_err(|_| KeyBindingsModelError::InvalidJson)?;
        let JsonValue::Array(entries) = root else {
            return Err(KeyBindingsModelError::ExpectedArray);
        };

        let mut pending = Vec::new();
        for entry in &entries {
            let JsonValue::Object(entry) = entry else {
                return Err(KeyBindingsModelError::ExpectedEntryObject);
            };
            for key in parse_entry_keys(entry.get("keys"))? {
                let (_, identity) = parse_key(&key)?;
                pending.push((identity, key));
            }
        }

        self.actions
            .layer_json(input)
            .map_err(|_| KeyBindingsModelError::Layering)?;

        for (identity, source) in pending {
            self.effective_keys.insert(identity, source);
        }
        Ok(())
    }

    #[must_use]
    pub fn keybinding_count(&self) -> usize {
        self.effective_keys.len()
    }

    #[must_use]
    pub fn action_name_for_key(&self, key: &str) -> Option<&str> {
        let source = self.source_key(key)?;
        self.actions.action_name_for_key(source)
    }

    #[must_use]
    pub fn action_id_for_chord(&self, chord: KeyChord) -> Option<&str> {
        let source = self.effective_keys.get(&chord.identity())?;
        self.actions.action_id_for_key(source)
    }

    fn source_key(&self, key: &str) -> Option<&str> {
        let (_, identity) = parse_key(key).ok()?;
        self.effective_keys.get(&identity).map(String::as_str)
    }
}

fn parse_entry_keys(value: Option<&JsonValue>) -> Result<Vec<String>, KeyBindingsModelError> {
    let value = value.ok_or(KeyBindingsModelError::ExpectedKeys)?;
    match value {
        JsonValue::String(key) => Ok(vec![key.trim().to_ascii_lowercase()]),
        JsonValue::Array(keys) => keys
            .iter()
            .map(|value| {
                value
                    .as_str()
                    .map(|key| key.trim().to_ascii_lowercase())
                    .ok_or(KeyBindingsModelError::ExpectedKeyString)
            })
            .collect(),
        _ => Err(KeyBindingsModelError::ExpectedKeys),
    }
}

fn parse_key(input: &str) -> Result<(KeyChord, KeyIdentity), KeyBindingsModelError> {
    let source = input.trim().to_ascii_lowercase();
    if source.is_empty() {
        return Err(KeyBindingsModelError::InvalidKeyChord);
    }

    let mut modifiers = 0u8;
    let mut vkey = 0i32;
    let mut scan_code = 0i32;
    let mut saw_key = false;

    for part in source.split('+') {
        match part {
            "ctrl" => modifiers |= MOD_CONTROL,
            "alt" => modifiers |= MOD_ALT,
            "shift" => modifiers |= MOD_SHIFT,
            "win" => modifiers |= MOD_WIN,
            _ => {
                if saw_key {
                    return Err(KeyBindingsModelError::InvalidKeyChord);
                }
                saw_key = true;

                if part.len() == 1 {
                    let byte = part.as_bytes()[0];
                    if byte.is_ascii_alphanumeric() {
                        vkey = i32::from(byte.to_ascii_uppercase());
                        continue;
                    }
                    if part == "`" {
                        vkey = VK_OEM_3;
                        continue;
                    }
                }

                if let Some(value) = parse_numeric_code(part, "vk(") {
                    vkey = value?;
                    continue;
                }
                if let Some(value) = parse_numeric_code(part, "sc(") {
                    scan_code = value?;
                    continue;
                }
                if let Some(value) = named_vkey(part) {
                    vkey = value;
                    continue;
                }
                return Err(KeyBindingsModelError::InvalidKeyChord);
            }
        }
    }

    if !saw_key || (vkey == 0 && scan_code == 0) {
        return Err(KeyBindingsModelError::InvalidKeyChord);
    }

    let chord = KeyChord::new(modifiers, vkey, scan_code);
    Ok((chord, chord.identity()))
}

fn parse_numeric_code(part: &str, prefix: &str) -> Option<Result<i32, KeyBindingsModelError>> {
    if !part.starts_with(prefix) || !part.ends_with(')') {
        return None;
    }
    let digits = &part[prefix.len()..part.len() - 1];
    let parsed = digits
        .parse::<u16>()
        .ok()
        .filter(|value| (1..=255).contains(value));
    Some(
        parsed
            .map(i32::from)
            .ok_or(KeyBindingsModelError::InvalidKeyChord),
    )
}

fn append_modifiers(modifiers: u8, result: &mut String) {
    for (flag, name) in [
        (MOD_WIN, "win"),
        (MOD_CONTROL, "ctrl"),
        (MOD_ALT, "alt"),
        (MOD_SHIFT, "shift"),
    ] {
        if modifiers & flag != 0 {
            if !result.is_empty() {
                result.push('+');
            }
            result.push_str(name);
        }
    }
    if !result.is_empty() {
        result.push('+');
    }
}

fn named_vkey(name: &str) -> Option<i32> {
    match name {
        "enter" => Some(VK_RETURN),
        "up" => Some(VK_UP),
        "down" => Some(VK_DOWN),
        "plus" => Some(VK_OEM_PLUS),
        _ => None,
    }
}

fn vkey_name(vkey: i32) -> Option<&'static str> {
    match vkey {
        VK_RETURN => Some("enter"),
        VK_UP => Some("up"),
        VK_DOWN => Some("down"),
        VK_OEM_PLUS => Some("plus"),
        _ => None,
    }
}
