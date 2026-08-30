//! Portable serialization owner for Windows Terminal actions and keybindings.
//!
//! The action map keeps the shared typed JSON tree as its serialization source
//! of truth while progressively owning portable `ActionMap` fixup semantics.

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ActionMapError {
    InvalidJson,
    ExpectedActionMap,
    ExpectedActionsArray,
    ExpectedKeybindingsArray,
    ExpectedEntryObject,
    ExpectedCommand,
    ExpectedActionName,
    ExpectedNestedCommandsArray,
    ExpectedSendInput,
    UnsupportedGeneratedIdArguments,
}

#[derive(Debug, Clone, PartialEq)]
pub struct ActionMapDocument {
    root: JsonValue,
}

impl ActionMapDocument {
    /// Parses an `ActionMap` serialization vector while retaining all typed JSON
    /// values for structure-identical projection.
    ///
    /// Microsoft serializes `ActionMap` either as a bare action array or through
    /// `GlobalAppSettings` as an object containing `actions` and `keybindings`.
    /// Both forms are accepted here and validated recursively for the portable
    /// structural contract used by `SerializationTests::Actions`.
    ///
    /// # Errors
    ///
    /// Returns [`ActionMapError`] for malformed JSON or an invalid action-map
    /// shape.
    pub fn from_json(input: &str) -> Result<Self, ActionMapError> {
        let root = settings_json::parse(input).map_err(|_| ActionMapError::InvalidJson)?;
        validate_root(&root)?;
        Ok(Self { root })
    }

    #[must_use]
    pub const fn to_json_value(&self) -> &JsonValue {
        &self.root
    }

    /// Reports whether Microsoft's user-origin `ActionMap` loader would mark the
    /// document as needing write-back fixups while layering these actions.
    ///
    /// Native `ActionMap::LayerJson` flags only ordinary user commands that
    /// either lack an `id` or still carry legacy inline `keys`. Nested command
    /// groups and iterable commands are deliberately excluded from generated-ID
    /// fixups.
    ///
    /// # Errors
    ///
    /// Returns [`ActionMapError`] when the action collection has an invalid
    /// shape.
    pub fn fixups_applied_during_load(&self) -> Result<bool, ActionMapError> {
        let actions = match &self.root {
            JsonValue::Array(actions) => actions.as_slice(),
            JsonValue::Object(root) => match root.get("actions") {
                Some(JsonValue::Array(actions)) => actions.as_slice(),
                Some(_) => return Err(ActionMapError::ExpectedActionsArray),
                None => return Ok(false),
            },
            _ => return Err(ActionMapError::ExpectedActionMap),
        };

        for action in actions {
            let JsonValue::Object(entry) = action else {
                return Err(ActionMapError::ExpectedEntryObject);
            };

            if entry.get("commands").is_some() || entry.get("iterateOn").is_some() {
                continue;
            }
            if entry.get("command").is_none() {
                continue;
            }

            if entry.get("id").is_none() || entry.get("keys").is_some() {
                return Ok(true);
            }
        }

        Ok(false)
    }

    /// Canonicalizes user `ActionMap` JSON to the modern actions/keybindings
    /// representation used by `SettingsLoader::FixupUserSettings`.
    ///
    /// The fixup migrates inline legacy keys, converts `unbound` actions to
    /// null-id keybindings, generates deterministic IDs, reuses matching inbox
    /// IDs for redundant user actions, and collapses duplicate command blocks
    /// that resolve to the same ID while preserving the first block's metadata.
    ///
    /// # Errors
    ///
    /// Returns [`ActionMapError`] for malformed action/keybinding shapes or an
    /// action whose generated-ID arguments are not yet owned by safe Rust.
    pub fn fixup_user_actions(&mut self, inbox: Option<&Self>) -> Result<bool, ActionMapError> {
        let original = self.root.clone();
        let inbox_actions = match inbox {
            Some(inbox) => collect_action_objects(inbox)?,
            None => Vec::new(),
        };

        let JsonValue::Object(root) = &mut self.root else {
            return Err(ActionMapError::ExpectedActionMap);
        };

        let actions = root
            .remove("actions")
            .ok_or(ActionMapError::ExpectedActionsArray)?;
        let JsonValue::Array(actions) = actions else {
            return Err(ActionMapError::ExpectedActionsArray);
        };

        let had_keybindings = root.contains_key("keybindings");
        let mut keybindings = match root.remove("keybindings") {
            Some(JsonValue::Array(keybindings)) => keybindings,
            Some(_) => return Err(ActionMapError::ExpectedKeybindingsArray),
            None => Vec::new(),
        };

        let mut modern_actions = Vec::new();
        for action in actions {
            let JsonValue::Object(mut entry) = action else {
                return Err(ActionMapError::ExpectedEntryObject);
            };

            if entry.get("commands").is_some() || entry.get("iterateOn").is_some() {
                modern_actions.push(JsonValue::Object(entry));
                continue;
            }

            let command = entry
                .get("command")
                .cloned()
                .ok_or(ActionMapError::ExpectedCommand)?;
            let keys = entry.remove("keys");

            if command.as_str() == Some("unbound") {
                if let Some(keys) = keys {
                    keybindings.push(keybinding(JsonValue::Null, keys));
                }
                continue;
            }

            let inbox_match = inbox_actions
                .iter()
                .find(|candidate| candidate.get("command") == Some(&command));

            let id = if let Some(id) = entry.get("id").and_then(JsonValue::as_str) {
                id.to_owned()
            } else if let Some(id) = inbox_match
                .and_then(|candidate| candidate.get("id"))
                .and_then(JsonValue::as_str)
            {
                id.to_owned()
            } else {
                generated_or_explicit_action_id(&JsonValue::Object(entry.clone()))?
            };

            entry.insert("id".to_owned(), JsonValue::String(id.clone()));

            if let Some(keys) = keys {
                keybindings.push(keybinding(JsonValue::String(id.clone()), keys));
            }

            let redundant_inbox = inbox_match.is_some()
                && entry
                    .keys()
                    .all(|key| matches!(key.as_str(), "command" | "id"));
            if redundant_inbox {
                continue;
            }

            let duplicate = modern_actions.iter().any(|existing| {
                existing
                    .as_object()
                    .and_then(|existing| existing.get("id"))
                    .and_then(JsonValue::as_str)
                    == Some(id.as_str())
            });
            if !duplicate {
                modern_actions.push(JsonValue::Object(entry));
            }
        }

        root.insert("actions".to_owned(), JsonValue::Array(modern_actions));
        if had_keybindings || !keybindings.is_empty() {
            root.insert("keybindings".to_owned(), JsonValue::Array(keybindings));
        }

        Ok(self.root != original)
    }

    /// Resolves the action ID associated with a key chord, including the legacy
    /// `keys`-inside-action form used by `SettingsLoader` before action fixup.
    ///
    /// If the action does not already have an explicit ID, this method applies
    /// Microsoft's portable `ActionAndArgs::GenerateID` rule. Argument hashing
    /// is currently owned for `sendInput`, the generated-ID family exercised by
    /// Microsoft's serialization contracts.
    ///
    /// # Errors
    ///
    /// Returns [`ActionMapError`] if the matching action has malformed or as-yet
    /// unsupported generated-ID arguments.
    pub fn action_id_for_key_chord(
        &self,
        key_chord: &str,
    ) -> Result<Option<String>, ActionMapError> {
        let JsonValue::Object(root) = &self.root else {
            return Ok(None);
        };

        if let Some(JsonValue::Array(keybindings)) = root.get("keybindings") {
            for binding in keybindings {
                let JsonValue::Object(binding) = binding else {
                    return Err(ActionMapError::ExpectedEntryObject);
                };
                if binding.get("keys").and_then(JsonValue::as_str) == Some(key_chord)
                    && let Some(id) = binding.get("id").and_then(JsonValue::as_str)
                {
                    return Ok(Some(id.to_owned()));
                }
            }
        }

        let Some(actions) = root.get("actions") else {
            return Ok(None);
        };
        let JsonValue::Array(actions) = actions else {
            return Err(ActionMapError::ExpectedActionsArray);
        };

        for action in actions {
            let JsonValue::Object(entry) = action else {
                return Err(ActionMapError::ExpectedEntryObject);
            };
            if entry.get("keys").and_then(JsonValue::as_str) == Some(key_chord) {
                return generated_or_explicit_action_id(action).map(Some);
            }
        }

        Ok(None)
    }
}

fn collect_action_objects(document: &ActionMapDocument) -> Result<Vec<JsonObject>, ActionMapError> {
    let actions = match &document.root {
        JsonValue::Array(actions) => actions.as_slice(),
        JsonValue::Object(root) => match root.get("actions") {
            Some(JsonValue::Array(actions)) => actions.as_slice(),
            Some(_) => return Err(ActionMapError::ExpectedActionsArray),
            None => return Ok(Vec::new()),
        },
        _ => return Err(ActionMapError::ExpectedActionMap),
    };

    actions
        .iter()
        .map(|action| match action {
            JsonValue::Object(action) => Ok(action.clone()),
            _ => Err(ActionMapError::ExpectedEntryObject),
        })
        .collect()
}

fn keybinding(id: JsonValue, keys: JsonValue) -> JsonValue {
    let mut binding = JsonObject::new();
    binding.insert("id".to_owned(), id);
    binding.insert("keys".to_owned(), keys);
    JsonValue::Object(binding)
}

fn validate_root(root: &JsonValue) -> Result<(), ActionMapError> {
    match root {
        JsonValue::Array(actions) => validate_actions(actions),
        JsonValue::Object(object) => {
            let mut has_surface = false;
            if let Some(actions) = object.get("actions") {
                has_surface = true;
                let JsonValue::Array(actions) = actions else {
                    return Err(ActionMapError::ExpectedActionsArray);
                };
                validate_actions(actions)?;
            }
            if let Some(keybindings) = object.get("keybindings") {
                has_surface = true;
                let JsonValue::Array(keybindings) = keybindings else {
                    return Err(ActionMapError::ExpectedKeybindingsArray);
                };
                validate_keybindings(keybindings)?;
            }
            if has_surface {
                Ok(())
            } else {
                Err(ActionMapError::ExpectedActionMap)
            }
        }
        _ => Err(ActionMapError::ExpectedActionMap),
    }
}

fn validate_actions(actions: &[JsonValue]) -> Result<(), ActionMapError> {
    for action in actions {
        validate_action_entry(action)?;
    }
    Ok(())
}

fn validate_action_entry(action: &JsonValue) -> Result<(), ActionMapError> {
    let JsonValue::Object(action) = action else {
        return Err(ActionMapError::ExpectedEntryObject);
    };

    if let Some(commands) = action.get("commands") {
        let JsonValue::Array(commands) = commands else {
            return Err(ActionMapError::ExpectedNestedCommandsArray);
        };
        for command in commands {
            validate_action_entry(command)?;
        }
        return Ok(());
    }

    let Some(command) = action.get("command") else {
        return Err(ActionMapError::ExpectedCommand);
    };
    match command {
        JsonValue::String(_) => Ok(()),
        JsonValue::Object(command) => match command.get("action") {
            Some(JsonValue::String(_)) => Ok(()),
            _ => Err(ActionMapError::ExpectedActionName),
        },
        _ => Err(ActionMapError::ExpectedCommand),
    }
}

fn validate_keybindings(keybindings: &[JsonValue]) -> Result<(), ActionMapError> {
    for keybinding in keybindings {
        if !matches!(keybinding, JsonValue::Object(_)) {
            return Err(ActionMapError::ExpectedEntryObject);
        }
    }
    Ok(())
}

fn generated_or_explicit_action_id(action: &JsonValue) -> Result<String, ActionMapError> {
    let JsonValue::Object(entry) = action else {
        return Err(ActionMapError::ExpectedEntryObject);
    };

    if let Some(id) = entry.get("id").and_then(JsonValue::as_str) {
        return Ok(id.to_owned());
    }

    let Some(command) = entry.get("command") else {
        return Err(ActionMapError::ExpectedCommand);
    };
    match command {
        JsonValue::String(action_name) => Ok(format!("User.{action_name}")),
        JsonValue::Object(command) => {
            let action_name = command
                .get("action")
                .and_then(JsonValue::as_str)
                .ok_or(ActionMapError::ExpectedActionName)?;

            if action_name == "sendInput" {
                let input = command
                    .get("input")
                    .and_then(JsonValue::as_str)
                    .ok_or(ActionMapError::ExpectedSendInput)?;
                return Ok(format!(
                    "User.sendInput.{:X}",
                    microsoft_send_input_hash(input)
                ));
            }

            if command.len() == 1 {
                Ok(format!("User.{action_name}"))
            } else {
                Err(ActionMapError::UnsupportedGeneratedIdArguments)
            }
        }
        _ => Err(ActionMapError::ExpectedCommand),
    }
}

fn microsoft_send_input_hash(input: &str) -> u32 {
    let mut bytes = Vec::with_capacity(input.encode_utf16().count() * 2);
    for unit in input.encode_utf16() {
        bytes.extend_from_slice(&unit.to_le_bytes());
    }

    if usize::BITS == 32 {
        til_hash32(&bytes, 0)
    } else {
        til_hash64(&bytes, 0) as u32
    }
}

fn read_u32(data: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes([
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
    ])
}

fn read_u64(data: &[u8], offset: usize) -> u64 {
    u64::from_le_bytes([
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
        data[offset + 4],
        data[offset + 5],
        data[offset + 6],
        data[offset + 7],
    ])
}

fn mix64(lhs: u64, rhs: u64) -> u64 {
    let product = u128::from(lhs) * u128::from(rhs);
    product as u64 ^ (product >> 64) as u64
}

fn til_hash64(data: &[u8], mut seed: u64) -> u64 {
    const S0: u64 = 0xa076_1d64_78bd_642f;
    const S1: u64 = 0xe703_7ed1_a0b4_28db;
    const S2: u64 = 0x8ebc_6af0_9c88_c6e3;
    const S3: u64 = 0x5899_65cc_7537_4cc3;

    let len = data.len();
    seed ^= S0;

    let (a, b) = if len <= 16 {
        if len >= 4 {
            let shift = (len >> 3) << 2;
            (
                (u64::from(read_u32(data, 0)) << 32) | u64::from(read_u32(data, shift)),
                (u64::from(read_u32(data, len - 4)) << 32)
                    | u64::from(read_u32(data, len - 4 - shift)),
            )
        } else if len > 0 {
            let a = (u64::from(data[0]) << 16)
                | (u64::from(data[len >> 1]) << 8)
                | u64::from(data[len - 1]);
            (a, 0)
        } else {
            (0, 0)
        }
    } else {
        let mut offset = 0usize;
        let mut remaining = len;

        if remaining > 48 {
            let mut seed1 = seed;
            let mut seed2 = seed;
            while remaining > 48 {
                seed = mix64(
                    read_u64(data, offset) ^ S1,
                    read_u64(data, offset + 8) ^ seed,
                );
                seed1 = mix64(
                    read_u64(data, offset + 16) ^ S2,
                    read_u64(data, offset + 24) ^ seed1,
                );
                seed2 = mix64(
                    read_u64(data, offset + 32) ^ S3,
                    read_u64(data, offset + 40) ^ seed2,
                );
                offset += 48;
                remaining -= 48;
            }
            seed ^= seed1 ^ seed2;
        }

        while remaining > 16 {
            seed = mix64(
                read_u64(data, offset) ^ S1,
                read_u64(data, offset + 8) ^ seed,
            );
            offset += 16;
            remaining -= 16;
        }

        (
            read_u64(data, offset + remaining - 16),
            read_u64(data, offset + remaining - 8),
        )
    };

    mix64(S1 ^ len as u64, mix64(a ^ S1, b ^ seed))
}

fn mix32(a: &mut u32, b: &mut u32) {
    let product = u64::from(*a ^ 0x53c5_ca59) * u64::from(*b ^ 0x7474_3c1b);
    *a = product as u32;
    *b = (product >> 32) as u32;
}

fn til_hash32(data: &[u8], mut seed: u32) -> u32 {
    let mut remaining = data.len();
    let mut offset = 0usize;
    let mut secondary = remaining as u32;
    mix32(&mut seed, &mut secondary);

    while remaining > 8 {
        seed ^= read_u32(data, offset);
        secondary ^= read_u32(data, offset + 4);
        mix32(&mut seed, &mut secondary);
        offset += 8;
        remaining -= 8;
    }

    if remaining >= 4 {
        seed ^= read_u32(data, offset);
        secondary ^= read_u32(data, offset + remaining - 4);
    } else if remaining > 0 {
        seed ^= (u32::from(data[offset]) << 16)
            | (u32::from(data[offset + (remaining >> 1)]) << 8)
            | u32::from(data[offset + remaining - 1]);
    }

    mix32(&mut seed, &mut secondary);
    mix32(&mut seed, &mut secondary);
    seed ^ secondary
}
