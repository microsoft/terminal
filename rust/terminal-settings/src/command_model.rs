//! Portable owner for `SettingsModel` command layering and commandline projection.
//!
//! Microsoft `Command::LayerJson` shares action argument semantics with
//! keybindings, but owns a distinct name-keyed command collection. This module
//! keeps that layering state portable while leaving `WinRT` resource projection at
//! the boundary.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CommandError {
    InvalidJson,
    ExpectedArray,
    ExpectedEntryObject,
    ExpectedCommand,
    ExpectedName,
    InvalidSplitSize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDirection {
    Left,
    Right,
    Up,
    Down,
    Automatic,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct LayeredCommands {
    commands: BTreeMap<String, JsonValue>,
    warnings: usize,
}

impl LayeredCommands {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Layers one Microsoft command array onto the current name-keyed command map.
    ///
    /// Explicit names layer by identity. Null commands remove an existing entry.
    /// Unnamed split-pane commands receive the same generated display names used
    /// by Microsoft's `SettingsModel` resources for the source vectors.
    ///
    /// # Errors
    ///
    /// Returns [`CommandError`] for malformed JSON or command/name shapes.
    pub fn layer_json(&mut self, input: &str) -> Result<(), CommandError> {
        let root = settings_json::parse(input).map_err(|_| CommandError::InvalidJson)?;
        let JsonValue::Array(entries) = root else {
            return Err(CommandError::ExpectedArray);
        };

        for entry in entries {
            let JsonValue::Object(entry) = entry else {
                return Err(CommandError::ExpectedEntryObject);
            };
            self.layer_entry(&entry)?;
        }
        Ok(())
    }

    #[must_use]
    pub fn command_count(&self) -> usize {
        self.commands.len()
    }

    #[must_use]
    pub fn warning_count(&self) -> usize {
        self.warnings
    }

    #[must_use]
    pub fn action_name(&self, name: &str) -> Option<&str> {
        action_name(self.commands.get(name)?)
    }

    #[must_use]
    pub fn split_direction(&self, name: &str) -> Option<SplitDirection> {
        let JsonValue::Object(command) = self.commands.get(name)? else {
            return None;
        };
        (command.get("action").and_then(JsonValue::as_str) == Some("splitPane"))
            .then(|| effective_split_direction(command))
    }

    #[must_use]
    pub fn split_size(&self, name: &str) -> Option<f64> {
        let JsonValue::Object(command) = self.commands.get(name)? else {
            return None;
        };
        if command.get("action").and_then(JsonValue::as_str) != Some("splitPane") {
            return None;
        }
        Some(
            command
                .get("size")
                .and_then(JsonValue::as_f64)
                .unwrap_or(0.5),
        )
    }

    #[must_use]
    pub fn commandline(&self, name: &str) -> Option<String> {
        let command = self.commands.get(name)?;
        let JsonValue::Object(command) = command else {
            return Some(String::new());
        };
        let action = command.get("action").and_then(JsonValue::as_str)?;
        if !matches!(action, "newTab" | "newWindow") {
            return None;
        }

        let mut parts = Vec::new();
        if let Some(profile) = command.get("profile").and_then(JsonValue::as_str) {
            parts.push(format!("--profile {}", quote_argument(profile)));
        }
        if let Some(directory) = command.get("startingDirectory").and_then(JsonValue::as_str) {
            parts.push(format!("--startingDirectory {}", quote_argument(directory)));
        }
        if let Some(title) = command.get("tabTitle").and_then(JsonValue::as_str) {
            parts.push(format!("--title {}", quote_argument(title)));
        }
        if let Some(commandline) = command.get("commandline").and_then(JsonValue::as_str) {
            parts.push(format!("-- \"{commandline}\""));
        }
        Some(parts.join(" "))
    }

    fn layer_entry(&mut self, entry: &JsonObject) -> Result<(), CommandError> {
        let command = entry.get("command").ok_or(CommandError::ExpectedCommand)?;
        let name = command_name(entry, command)?;

        if matches!(command, JsonValue::Null) {
            self.commands.remove(&name);
            return Ok(());
        }

        if let JsonValue::Object(command) = command
            && command.get("action").and_then(JsonValue::as_str) == Some("splitPane")
            && let Some(size) = command.get("size").and_then(JsonValue::as_f64)
            && (!(0.0..1.0).contains(&size) || size == 0.0)
        {
            self.warnings += 1;
            return Ok(());
        }

        self.commands.insert(name, command.clone());
        Ok(())
    }
}

fn action_name(command: &JsonValue) -> Option<&str> {
    match command {
        JsonValue::String(name) => Some(name),
        JsonValue::Object(command) => command.get("action").and_then(JsonValue::as_str),
        _ => None,
    }
}

fn command_name(entry: &JsonObject, command: &JsonValue) -> Result<String, CommandError> {
    match entry.get("name") {
        Some(JsonValue::String(name)) => Ok(name.clone()),
        Some(JsonValue::Object(resource)) => {
            match resource.get("key").and_then(JsonValue::as_str) {
                Some("DuplicateTabCommandKey") => Ok("Duplicate tab".to_owned()),
                _ => Err(CommandError::ExpectedName),
            }
        }
        Some(_) => Err(CommandError::ExpectedName),
        None => generated_name(command).ok_or(CommandError::ExpectedName),
    }
}

fn generated_name(command: &JsonValue) -> Option<String> {
    let JsonValue::Object(command) = command else {
        return None;
    };
    if command.get("action").and_then(JsonValue::as_str) != Some("splitPane") {
        return None;
    }

    let suffix = match effective_split_direction(command) {
        SplitDirection::Left => Some("left"),
        SplitDirection::Right
            if command.get("split").and_then(JsonValue::as_str) == Some("right") =>
        {
            Some("right")
        }
        SplitDirection::Up => Some("up"),
        SplitDirection::Down
            if command.get("split").and_then(JsonValue::as_str) == Some("down") =>
        {
            Some("down")
        }
        _ => None,
    };
    Some(match suffix {
        Some(suffix) => format!("Split pane, split: {suffix}"),
        None => "Split pane".to_owned(),
    })
}

fn effective_split_direction(command: &JsonObject) -> SplitDirection {
    match command.get("split").and_then(JsonValue::as_str) {
        Some("vertical" | "right") => SplitDirection::Right,
        Some("horizontal" | "down") => SplitDirection::Down,
        Some("left") => SplitDirection::Left,
        Some("up") => SplitDirection::Up,
        _ => SplitDirection::Automatic,
    }
}

fn quote_argument(value: &str) -> String {
    let mut output = String::from("\"");
    let mut slashes = 0usize;

    for ch in value.chars() {
        if ch == '\\' {
            slashes += 1;
            continue;
        }

        if ch == '"' {
            output.push_str(&"\\".repeat(slashes * 2 + 1));
            output.push('"');
        } else {
            output.push_str(&"\\".repeat(slashes));
            if ch == ';' {
                output.push('\\');
            }
            output.push(ch);
        }
        slashes = 0;
    }

    output.push_str(&"\\".repeat(slashes * 2));
    output.push('"');
    output
}
