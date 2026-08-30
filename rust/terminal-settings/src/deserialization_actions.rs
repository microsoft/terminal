//! Portable `SettingsModel` action/command deserialization aggregate.
//!
//! Microsoft `CascadiaSettings` layers `actions` and `keybindings` through one
//! `ActionMap`. This owner keeps the portable identity/name/key semantics together
//! while leaving `WinRT` projection and active keyboard-layout translation at the
//! platform boundary.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationActionWarning {
    FailedToParseSubCommands,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeserializationActionError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedActionsArray,
    ExpectedEntryObject,
    ExpectedKeybindingsArray,
    ExpectedKeyString,
    ExpectedIdString,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDirection {
    Right,
    Down,
    Automatic,
}

#[derive(Debug, Clone, PartialEq)]
struct ActionRecord {
    command: Option<JsonValue>,
    nested_count: usize,
    persistent: bool,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct DeserializedActionMap {
    actions: BTreeMap<String, ActionRecord>,
    names: BTreeMap<String, String>,
    keys: BTreeMap<String, Option<String>>,
    warnings: Vec<DeserializationActionWarning>,
    generated_id: u64,
}

impl DeserializedActionMap {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Layers one complete `SettingsModel` JSON document onto the current action map.
    ///
    /// `actions` are processed before `keybindings`, matching the product's
    /// ability to bind a key to an action ID declared in the same or an earlier
    /// layer.
    ///
    /// # Errors
    ///
    /// Returns [`DeserializationActionError`] for malformed JSON or malformed
    /// action/keybinding container shapes.
    pub fn layer_settings(&mut self, input: &str) -> Result<(), DeserializationActionError> {
        let root =
            settings_json::parse(input).map_err(|_| DeserializationActionError::InvalidJson)?;
        let JsonValue::Object(root) = root else {
            return Err(DeserializationActionError::ExpectedRootObject);
        };

        if let Some(actions) = root.get("actions") {
            let JsonValue::Array(actions) = actions else {
                return Err(DeserializationActionError::ExpectedActionsArray);
            };
            for entry in actions {
                let JsonValue::Object(entry) = entry else {
                    return Err(DeserializationActionError::ExpectedEntryObject);
                };
                self.layer_action_entry(entry)?;
            }
        }

        if let Some(keybindings) = root.get("keybindings") {
            let JsonValue::Array(keybindings) = keybindings else {
                return Err(DeserializationActionError::ExpectedKeybindingsArray);
            };
            for entry in keybindings {
                let JsonValue::Object(entry) = entry else {
                    return Err(DeserializationActionError::ExpectedEntryObject);
                };
                self.layer_keybinding_entry(entry)?;
            }
        }

        Ok(())
    }

    #[must_use]
    pub fn keybinding_count(&self) -> usize {
        self.keys.values().filter(|value| value.is_some()).count()
    }

    #[must_use]
    pub fn name_count(&self) -> usize {
        self.names.len()
    }

    #[must_use]
    pub fn warnings(&self) -> &[DeserializationActionWarning] {
        &self.warnings
    }

    /// `CascadiaSettings` adds one aggregate `AtLeastOneKeybindingWarning` entry
    /// ahead of the concrete `ActionMap` warnings.
    #[must_use]
    pub fn settings_warning_count(&self) -> usize {
        if self.warnings.is_empty() {
            0
        } else {
            self.warnings.len() + 1
        }
    }

    #[must_use]
    pub fn action_id_for_key(&self, key: &str) -> Option<&str> {
        self.keys
            .get(&normalize_key(key))
            .and_then(Option::as_deref)
    }

    #[must_use]
    pub fn action_name_for_key(&self, key: &str) -> Option<&str> {
        let id = self.action_id_for_key(key)?;
        self.action_name_for_id(id)
    }

    #[must_use]
    pub fn split_direction_for_key(&self, key: &str) -> Option<SplitDirection> {
        let id = self.action_id_for_key(key)?;
        let command = self.actions.get(id)?.command.as_ref()?;
        match command {
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
            JsonValue::String(action) if action == "splitPane" => Some(SplitDirection::Automatic),
            _ => None,
        }
    }

    #[must_use]
    pub fn name_action(&self, name: &str) -> Option<&str> {
        let id = self.names.get(name)?;
        self.action_name_for_id(id)
    }

    #[must_use]
    pub fn name_has_nested_commands(&self, name: &str) -> bool {
        self.names
            .get(name)
            .and_then(|id| self.actions.get(id))
            .is_some_and(|record| record.nested_count != 0)
    }

    #[must_use]
    pub fn nested_command_count(&self, name: &str) -> Option<usize> {
        let id = self.names.get(name)?;
        self.actions.get(id).map(|record| record.nested_count)
    }

    #[must_use]
    pub fn key_binding_for_action(&self, id: &str) -> Option<&str> {
        self.keys
            .iter()
            .find_map(|(key, binding)| (binding.as_deref() == Some(id)).then_some(key.as_str()))
    }

    fn action_name_for_id(&self, id: &str) -> Option<&str> {
        let command = self.actions.get(id)?.command.as_ref()?;
        action_name(command)
    }

    fn layer_action_entry(&mut self, entry: &JsonObject) -> Result<(), DeserializationActionError> {
        if let Some(commands) = entry.get("commands") {
            return self.layer_nested_entry(entry, commands);
        }

        let keys = parse_keys(entry.get("keys"))?;
        let name = entry.get("name").and_then(JsonValue::as_str);
        let explicit_id = match entry.get("id") {
            Some(JsonValue::String(id)) => Some(id.clone()),
            Some(_) => return Err(DeserializationActionError::ExpectedIdString),
            None => None,
        };
        let command = entry.get("command");

        if matches!(command, None | Some(JsonValue::Null)) {
            for key in keys {
                self.unbind_key(key);
            }
            if let Some(name) = name
                && let Some(id) = self.names.remove(name)
            {
                self.actions.remove(&id);
            }
            return Ok(());
        }

        let Some(command) = command else {
            return Ok(());
        };
        let command = command.clone();
        let existing_name_id = name.and_then(|name| self.names.get(name).cloned());
        let materialize = explicit_id.is_some() || !keys.is_empty() || existing_name_id.is_some();
        if !materialize {
            return Ok(());
        }

        let persistent = explicit_id.is_some() || existing_name_id.is_some();
        let id = explicit_id
            .or(existing_name_id)
            .unwrap_or_else(|| self.next_generated_id());
        self.actions.insert(
            id.clone(),
            ActionRecord {
                command: Some(command),
                nested_count: 0,
                persistent,
            },
        );

        if let Some(name) = name {
            self.names.insert(name.to_owned(), id.clone());
        }
        for key in keys {
            self.keys.insert(key, Some(id.clone()));
        }
        Ok(())
    }

    fn layer_nested_entry(
        &mut self,
        entry: &JsonObject,
        commands: &JsonValue,
    ) -> Result<(), DeserializationActionError> {
        let Some(name) = entry.get("name").and_then(JsonValue::as_str) else {
            // Microsoft intentionally ignores unnamed nested commands.
            return Ok(());
        };

        if matches!(commands, JsonValue::Null) {
            if let Some(id) = self.names.remove(name) {
                self.actions.remove(&id);
            }
            return Ok(());
        }

        let JsonValue::Array(children) = commands else {
            self.warnings
                .push(DeserializationActionWarning::FailedToParseSubCommands);
            return Ok(());
        };

        let valid = children.iter().all(|child| {
            let JsonValue::Object(child) = child else {
                return false;
            };
            child.get("name").and_then(JsonValue::as_str).is_some()
                && (child.get("command").is_some() || child.get("commands").is_some())
        });
        if !valid {
            self.warnings
                .push(DeserializationActionWarning::FailedToParseSubCommands);
            return Ok(());
        }

        let id = self
            .names
            .get(name)
            .cloned()
            .or_else(|| {
                entry
                    .get("id")
                    .and_then(JsonValue::as_str)
                    .map(str::to_owned)
            })
            .unwrap_or_else(|| self.next_generated_id());
        self.names.insert(name.to_owned(), id.clone());
        self.actions.insert(
            id,
            ActionRecord {
                command: None,
                nested_count: children.len(),
                persistent: true,
            },
        );
        Ok(())
    }

    fn layer_keybinding_entry(
        &mut self,
        entry: &JsonObject,
    ) -> Result<(), DeserializationActionError> {
        let keys = parse_keys(entry.get("keys"))?;
        let id = match entry.get("id") {
            Some(JsonValue::String(id)) => id.clone(),
            Some(_) => return Err(DeserializationActionError::ExpectedIdString),
            None => return Ok(()),
        };
        for key in keys {
            self.keys.insert(key, Some(id.clone()));
        }
        Ok(())
    }

    fn unbind_key(&mut self, key: String) {
        let previous = self.keys.insert(key, None).flatten();
        let Some(id) = previous else {
            return;
        };
        let still_bound = self
            .keys
            .values()
            .any(|binding| binding.as_deref() == Some(id.as_str()));
        let generated = self
            .actions
            .get(&id)
            .is_some_and(|record| !record.persistent);
        if !still_bound && generated {
            self.actions.remove(&id);
            self.names.retain(|_, value| value != &id);
        }
    }

    fn next_generated_id(&mut self) -> String {
        self.generated_id += 1;
        format!("User.Deserialized.{}", self.generated_id)
    }
}

fn parse_keys(value: Option<&JsonValue>) -> Result<Vec<String>, DeserializationActionError> {
    match value {
        None => Ok(Vec::new()),
        Some(JsonValue::String(key)) => Ok(vec![normalize_key(key)]),
        Some(JsonValue::Array(keys)) => keys
            .iter()
            .map(|key| {
                key.as_str()
                    .map(normalize_key)
                    .ok_or(DeserializationActionError::ExpectedKeyString)
            })
            .collect(),
        Some(_) => Err(DeserializationActionError::ExpectedKeyString),
    }
}

fn normalize_key(key: &str) -> String {
    key.trim().to_ascii_lowercase()
}

fn action_name(command: &JsonValue) -> Option<&str> {
    match command {
        JsonValue::String(action) => Some(action.as_str()),
        JsonValue::Object(command) => command.get("action").and_then(JsonValue::as_str),
        _ => None,
    }
}
