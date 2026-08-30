//! Portable layered ActionMap/keybinding semantics used by `SettingsModel` tests.
//!
//! `ActionMapDocument` owns serialized action-map projection. This module owns
//! the mutable layering state that Microsoft's `ActionMap::LayerJson` exposes:
//! key-chord override, explicit unbind, action deduplication and bidirectional
//! key/action lookup. The representation intentionally stays independent of
//! `WinRT` `KeyChord`/`ShortcutAction` projection.

use std::collections::{BTreeMap, BTreeSet};

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum KeyBindingError {
    InvalidJson,
    ExpectedArray,
    ExpectedEntryObject,
    ExpectedKeys,
    ExpectedKeyString,
    ExpectedIdString,
    InvalidActionArguments,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDirection {
    Right,
    Down,
    Automatic,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandPaletteLaunchMode {
    Action,
    CommandLine,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MoveTabDirection {
    Forward,
    Backward,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct LayeredActionMap {
    key_map: BTreeMap<String, Option<String>>,
    actions: BTreeMap<String, JsonValue>,
    explicitly_unbound: BTreeSet<String>,
}

impl LayeredActionMap {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Layers one Microsoft `ActionMap` JSON array onto the current state.
    ///
    /// Later keybindings replace earlier bindings for the same chord. Invalid,
    /// unknown and explicit `unbound` commands retain the chord in the key map
    /// with no action, matching Microsoft's explicit-unbind behavior.
    ///
    /// # Errors
    ///
    /// Returns [`KeyBindingError`] when the JSON, keybinding shape, or a
    /// recognized action's typed arguments are invalid.
    pub fn layer_json(&mut self, input: &str) -> Result<(), KeyBindingError> {
        let root = settings_json::parse(input).map_err(|_| KeyBindingError::InvalidJson)?;
        let JsonValue::Array(entries) = root else {
            return Err(KeyBindingError::ExpectedArray);
        };

        for entry in entries {
            let JsonValue::Object(entry) = entry else {
                return Err(KeyBindingError::ExpectedEntryObject);
            };
            self.layer_entry(&entry)?;
        }
        Ok(())
    }

    #[must_use]
    pub fn keybinding_count(&self) -> usize {
        self.key_map.len()
    }

    #[must_use]
    pub fn action_count(&self) -> usize {
        self.actions.len()
    }

    #[must_use]
    pub fn action_id_for_key(&self, key: &str) -> Option<&str> {
        self.key_map
            .get(&normalize_key(key))
            .and_then(Option::as_deref)
    }

    #[must_use]
    pub fn action_for_key(&self, key: &str) -> Option<&JsonValue> {
        let id = self.action_id_for_key(key)?;
        self.actions.get(id)
    }

    #[must_use]
    pub fn action_name_for_key(&self, key: &str) -> Option<&str> {
        action_name(self.action_for_key(key)?)
    }

    /// Returns Microsoft's effective `CopyText` `singleLine` argument for a key.
    /// A string `copy` command and an object without the member both use false.
    #[must_use]
    pub fn copy_single_line_for_key(&self, key: &str) -> Option<bool> {
        match self.action_for_key(key)? {
            JsonValue::String(name) if name == "copy" => Some(false),
            JsonValue::Object(command)
                if command.get("action").and_then(JsonValue::as_str) == Some("copy") =>
            {
                Some(
                    command
                        .get("singleLine")
                        .and_then(JsonValue::as_bool)
                        .unwrap_or(false),
                )
            }
            _ => None,
        }
    }

    /// Returns the effective `newTab` profile index. The outer Option indicates
    /// that the bound action is `newTab`; the inner Option models Microsoft's
    /// nullable `ProfileIndex` argument.
    #[must_use]
    pub fn new_tab_index_for_key(&self, key: &str) -> Option<Option<i32>> {
        match self.action_for_key(key)? {
            JsonValue::String(name) if name == "newTab" => Some(None),
            JsonValue::Object(command)
                if command.get("action").and_then(JsonValue::as_str) == Some("newTab") =>
            {
                let index = command.get("index").and_then(JsonValue::as_f64);
                Some(index.map(|value| value as i32))
            }
            _ => None,
        }
    }

    #[must_use]
    pub fn adjust_font_delta_for_key(&self, key: &str) -> Option<i32> {
        let JsonValue::Object(command) = self.action_for_key(key)? else {
            return None;
        };
        (command.get("action").and_then(JsonValue::as_str) == Some("adjustFontSize"))
            .then(|| {
                command
                    .get("delta")
                    .and_then(JsonValue::as_f64)
                    .map(|v| v as i32)
            })
            .flatten()
    }

    #[must_use]
    pub fn split_direction_for_key(&self, key: &str) -> Option<SplitDirection> {
        match self.action_for_key(key)? {
            JsonValue::String(name) if name == "splitPane" => Some(SplitDirection::Automatic),
            JsonValue::Object(command)
                if command.get("action").and_then(JsonValue::as_str) == Some("splitPane") =>
            {
                match command.get("split").and_then(JsonValue::as_str) {
                    Some("vertical") => Some(SplitDirection::Right),
                    Some("horizontal") => Some(SplitDirection::Down),
                    Some("auto") | None => Some(SplitDirection::Automatic),
                    Some(_) => None,
                }
            }
            _ => None,
        }
    }

    /// Returns a Windows COLORREF value (0x00BBGGRR) when `SetTabColor` carries
    /// a color. The inner None represents Microsoft's nullable/default color.
    #[must_use]
    pub fn tab_color_for_key(&self, key: &str) -> Option<Option<u32>> {
        match self.action_for_key(key)? {
            JsonValue::String(name) if name == "setTabColor" => Some(None),
            JsonValue::Object(command)
                if command.get("action").and_then(JsonValue::as_str) == Some("setTabColor") =>
            {
                match command.get("color") {
                    None | Some(JsonValue::Null) => Some(None),
                    Some(JsonValue::String(color)) => parse_colorref(color).map(Some),
                    _ => None,
                }
            }
            _ => None,
        }
    }

    /// Returns rowsToScroll for scrollUp/scrollDown. The inner None means the
    /// property was omitted and Microsoft's default scrolling amount applies.
    #[must_use]
    pub fn rows_to_scroll_for_key(&self, key: &str) -> Option<Option<u32>> {
        let action = self.action_for_key(key)?;
        match action {
            JsonValue::String(name) if matches!(name.as_str(), "scrollUp" | "scrollDown") => {
                Some(None)
            }
            JsonValue::Object(command)
                if command
                    .get("action")
                    .and_then(JsonValue::as_str)
                    .is_some_and(|name| matches!(name, "scrollUp" | "scrollDown")) =>
            {
                Some(
                    command
                        .get("rowsToScroll")
                        .and_then(JsonValue::as_f64)
                        .map(|v| v as u32),
                )
            }
            _ => None,
        }
    }

    #[must_use]
    pub fn command_palette_launch_mode_for_key(
        &self,
        key: &str,
    ) -> Option<CommandPaletteLaunchMode> {
        match self.action_for_key(key)? {
            JsonValue::String(name) if name == "commandPalette" => {
                Some(CommandPaletteLaunchMode::Action)
            }
            JsonValue::Object(command)
                if command.get("action").and_then(JsonValue::as_str) == Some("commandPalette") =>
            {
                match command.get("launchMode").and_then(JsonValue::as_str) {
                    None | Some("action") => Some(CommandPaletteLaunchMode::Action),
                    Some("commandLine") => Some(CommandPaletteLaunchMode::CommandLine),
                    Some(_) => None,
                }
            }
            _ => None,
        }
    }

    #[must_use]
    pub fn move_tab_direction_for_key(&self, key: &str) -> Option<MoveTabDirection> {
        let JsonValue::Object(command) = self.action_for_key(key)? else {
            return None;
        };
        if command.get("action").and_then(JsonValue::as_str) != Some("moveTab") {
            return None;
        }
        match command.get("direction").and_then(JsonValue::as_str) {
            Some("forward") => Some(MoveTabDirection::Forward),
            Some("backward") => Some(MoveTabDirection::Backward),
            _ => None,
        }
    }

    #[must_use]
    pub fn semantic_hash_for_key(&self, key: &str) -> Option<u64> {
        self.action_for_key(key).map(semantic_hash)
    }

    #[must_use]
    pub fn is_key_explicitly_unbound(&self, key: &str) -> bool {
        self.explicitly_unbound.contains(&normalize_key(key))
    }

    #[must_use]
    pub fn key_binding_for_action(&self, id: &str) -> Option<&str> {
        self.key_map
            .iter()
            .find_map(|(key, bound)| (bound.as_deref() == Some(id)).then_some(key.as_str()))
    }

    fn layer_entry(&mut self, entry: &JsonObject) -> Result<(), KeyBindingError> {
        let keys = parse_keys(entry.get("keys"))?;
        let command = entry.get("command");

        let Some(command) = command.filter(|value| recognized_command(value)) else {
            self.unbind(keys);
            return Ok(());
        };

        if !validate_action_arguments(command)? {
            self.unbind(keys);
            return Ok(());
        }

        let id = match entry.get("id") {
            Some(JsonValue::String(id)) => id.clone(),
            Some(_) => return Err(KeyBindingError::ExpectedIdString),
            None => generated_semantic_id(command),
        };

        self.actions
            .entry(id.clone())
            .or_insert_with(|| command.clone());
        for key in keys {
            self.key_map.insert(key.clone(), Some(id.clone()));
            self.explicitly_unbound.remove(&key);
        }
        Ok(())
    }

    fn unbind(&mut self, keys: Vec<String>) {
        for key in keys {
            self.key_map.insert(key.clone(), None);
            self.explicitly_unbound.insert(key);
        }
    }
}

fn parse_keys(value: Option<&JsonValue>) -> Result<Vec<String>, KeyBindingError> {
    let value = value.ok_or(KeyBindingError::ExpectedKeys)?;
    match value {
        JsonValue::String(key) => Ok(vec![normalize_key(key)]),
        JsonValue::Array(keys) => keys
            .iter()
            .map(|key| {
                key.as_str()
                    .map(normalize_key)
                    .ok_or(KeyBindingError::ExpectedKeyString)
            })
            .collect(),
        _ => Err(KeyBindingError::ExpectedKeys),
    }
}

fn normalize_key(key: &str) -> String {
    key.trim().to_ascii_lowercase()
}

fn recognized_command(command: &JsonValue) -> bool {
    match command {
        JsonValue::String(name) => is_known_action(name),
        JsonValue::Object(command) => command
            .get("action")
            .and_then(JsonValue::as_str)
            .is_some_and(is_known_action),
        JsonValue::Null | JsonValue::Bool(_) | JsonValue::Number(_) | JsonValue::Array(_) => false,
    }
}

fn validate_action_arguments(command: &JsonValue) -> Result<bool, KeyBindingError> {
    match command {
        JsonValue::String(name) => return Ok(name != "moveTab"),
        JsonValue::Object(command) => {
            let Some(action) = command.get("action").and_then(JsonValue::as_str) else {
                return Ok(false);
            };
            match action {
                "moveTab" => match command.get("direction").and_then(JsonValue::as_str) {
                    Some("forward" | "backward") => {}
                    _ => return Err(KeyBindingError::InvalidActionArguments),
                },
                "commandPalette" => {
                    if command
                        .get("launchMode")
                        .and_then(JsonValue::as_str)
                        .is_some_and(|mode| !matches!(mode, "action" | "commandLine"))
                    {
                        return Err(KeyBindingError::InvalidActionArguments);
                    }
                }
                "scrollUp" | "scrollDown" => {
                    if let Some(value) = command.get("rowsToScroll") {
                        let Some(rows) = value.as_f64() else {
                            return Err(KeyBindingError::InvalidActionArguments);
                        };
                        if rows < 0.0 || rows.fract() != 0.0 {
                            return Err(KeyBindingError::InvalidActionArguments);
                        }
                    }
                }
                "splitPane" => {
                    if command
                        .get("split")
                        .and_then(JsonValue::as_str)
                        .is_some_and(|direction| {
                            !matches!(direction, "vertical" | "horizontal" | "auto")
                        })
                    {
                        return Err(KeyBindingError::InvalidActionArguments);
                    }
                }
                "setTabColor" => {
                    if let Some(value) = command.get("color") {
                        match value {
                            JsonValue::Null => {}
                            JsonValue::String(color) if parse_colorref(color).is_some() => {}
                            _ => return Err(KeyBindingError::InvalidActionArguments),
                        }
                    }
                }
                _ => {}
            }
        }
        _ => return Ok(false),
    }
    Ok(true)
}

fn parse_colorref(color: &str) -> Option<u32> {
    let hex = color.strip_prefix('#')?;
    if hex.len() != 6 {
        return None;
    }
    let rgb = u32::from_str_radix(hex, 16).ok()?;
    let red = (rgb >> 16) & 0xff;
    let green = (rgb >> 8) & 0xff;
    let blue = rgb & 0xff;
    Some(red | (green << 8) | (blue << 16))
}

fn is_known_action(name: &str) -> bool {
    matches!(
        name,
        "adjustFontSize"
            | "closeWindow"
            | "commandPalette"
            | "copy"
            | "globalSummon"
            | "moveTab"
            | "newTab"
            | "paste"
            | "quakeMode"
            | "scrollDown"
            | "scrollUp"
            | "setTabColor"
            | "splitPane"
    )
}

fn action_name(command: &JsonValue) -> Option<&str> {
    match command {
        JsonValue::String(name) => Some(name.as_str()),
        JsonValue::Object(command) => command.get("action").and_then(JsonValue::as_str),
        _ => None,
    }
}

fn generated_semantic_id(command: &JsonValue) -> String {
    let name = action_name(command).unwrap_or("unbound");
    format!("User.{name}.{:X}", semantic_hash(command))
}

fn semantic_hash(value: &JsonValue) -> u64 {
    let mut hash = 0xcbf2_9ce4_8422_2325u64;
    write_semantic_bytes(value, &mut |byte| {
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(0x0000_0100_0000_01b3);
    });
    hash
}

fn write_semantic_bytes(value: &JsonValue, sink: &mut impl FnMut(u8)) {
    match value {
        JsonValue::Null => sink(b'n'),
        JsonValue::Bool(value) => {
            sink(b'b');
            sink(u8::from(*value));
        }
        JsonValue::Number(value) => {
            sink(b'd');
            for byte in value.to_bits().to_le_bytes() {
                sink(byte);
            }
        }
        JsonValue::String(value) => {
            sink(b's');
            write_len(value.len(), sink);
            for byte in value.as_bytes() {
                sink(*byte);
            }
        }
        JsonValue::Array(values) => {
            sink(b'a');
            write_len(values.len(), sink);
            for value in values {
                write_semantic_bytes(value, sink);
            }
        }
        JsonValue::Object(values) => {
            sink(b'o');
            write_len(values.len(), sink);
            for (key, value) in values {
                write_len(key.len(), sink);
                for byte in key.as_bytes() {
                    sink(*byte);
                }
                write_semantic_bytes(value, sink);
            }
        }
    }
}

fn write_len(len: usize, sink: &mut impl FnMut(u8)) {
    for byte in len.to_le_bytes() {
        sink(byte);
    }
}
