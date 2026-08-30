//! Portable command-palette expansion for iterable and nested settings actions.
//!
//! Windows Terminal keeps the source command map intact and builds a second
//! expanded command tree for command-palette projection. This owner performs
//! that deterministic expansion without `WinRT` or XAML dependencies.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandExpansionError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedActionsArray,
    ExpectedEntryObject,
    ExpectedCommandObject,
    ExpectedCommandsArray,
    ExpectedCollectionArray,
    ExpectedName,
    ExpectedString,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PaletteAction {
    Invalid,
    SplitPane,
    NewTab,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PaletteSplitDirection {
    Automatic,
    Right,
    Down,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PaletteCommand {
    name: String,
    action: PaletteAction,
    split_direction: Option<PaletteSplitDirection>,
    profile: String,
    commandline: String,
    nested: Vec<PaletteCommand>,
}

impl PaletteCommand {
    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub const fn action(&self) -> PaletteAction {
        self.action
    }

    #[must_use]
    pub const fn split_direction(&self) -> Option<PaletteSplitDirection> {
        self.split_direction
    }

    #[must_use]
    pub fn profile(&self) -> &str {
        &self.profile
    }

    #[must_use]
    pub fn commandline(&self) -> &str {
        &self.commandline
    }

    #[must_use]
    pub fn nested(&self) -> &[PaletteCommand] {
        &self.nested
    }

    #[must_use]
    pub fn nested_named(&self, name: &str) -> Option<&PaletteCommand> {
        self.nested.iter().find(|command| command.name == name)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ExpandedCommandSettings {
    active_profile_count: usize,
    source_commands: Vec<PaletteCommand>,
    expanded_commands: Vec<PaletteCommand>,
}

impl ExpandedCommandSettings {
    /// Parses settings and owns the portable command-palette expansion pass.
    ///
    /// The source command tree keeps iterable placeholders intact. The expanded
    /// tree recursively projects `profiles` and `schemes`, including iterable
    /// groups nested inside groups and groups nested inside iterable entries.
    /// String substitution happens on typed JSON values, so names containing
    /// quotes cannot corrupt a reconstructed JSON document.
    ///
    /// # Errors
    ///
    /// Returns [`CommandExpansionError`] when the settings/action shapes used by
    /// this owner are malformed.
    pub fn from_json(input: &str) -> Result<Self, CommandExpansionError> {
        let root = settings_json::parse(input).map_err(|_| CommandExpansionError::InvalidJson)?;
        let root = root
            .as_object()
            .ok_or(CommandExpansionError::ExpectedRootObject)?;
        let actions = action_entries(root)?;
        let active_profile_count = collection_names(root, "profiles")?.len();
        let empty_context = BTreeMap::new();

        let source_commands = actions
            .iter()
            .map(|entry| parse_template_entry(entry, &empty_context))
            .collect::<Result<Vec<_>, _>>()?;

        let mut expanded_commands = Vec::new();
        for entry in actions {
            expanded_commands.extend(expand_entry(entry, root, &empty_context)?);
        }

        Ok(Self {
            active_profile_count,
            source_commands,
            expanded_commands,
        })
    }

    #[must_use]
    pub const fn active_profile_count(&self) -> usize {
        self.active_profile_count
    }

    #[must_use]
    pub const fn warning_count(&self) -> usize {
        0
    }

    #[must_use]
    pub fn source_commands(&self) -> &[PaletteCommand] {
        &self.source_commands
    }

    #[must_use]
    pub fn expanded_commands(&self) -> &[PaletteCommand] {
        &self.expanded_commands
    }
}

fn action_entries(root: &JsonObject) -> Result<&[JsonValue], CommandExpansionError> {
    match root.get("actions") {
        None => Ok(&[]),
        Some(JsonValue::Array(actions)) => Ok(actions),
        Some(_) => Err(CommandExpansionError::ExpectedActionsArray),
    }
}

fn collection_names(
    root: &JsonObject,
    collection: &str,
) -> Result<Vec<String>, CommandExpansionError> {
    let Some(value) = root.get(collection) else {
        return Ok(Vec::new());
    };
    let values = if collection == "profiles" {
        match value {
            JsonValue::Array(values) => values.as_slice(),
            JsonValue::Object(object) => match object.get("list") {
                Some(JsonValue::Array(values)) => values.as_slice(),
                None => &[],
                Some(_) => return Err(CommandExpansionError::ExpectedCollectionArray),
            },
            _ => return Err(CommandExpansionError::ExpectedCollectionArray),
        }
    } else {
        value
            .as_array()
            .ok_or(CommandExpansionError::ExpectedCollectionArray)?
    };

    values
        .iter()
        .map(|value| {
            value
                .as_object()
                .ok_or(CommandExpansionError::ExpectedEntryObject)?
                .get("name")
                .and_then(JsonValue::as_str)
                .map(str::to_owned)
                .ok_or(CommandExpansionError::ExpectedName)
        })
        .collect()
}

fn expand_entry(
    value: &JsonValue,
    root: &JsonObject,
    context: &BTreeMap<String, String>,
) -> Result<Vec<PaletteCommand>, CommandExpansionError> {
    let entry = value
        .as_object()
        .ok_or(CommandExpansionError::ExpectedEntryObject)?;

    if let Some(iterate_on) = entry.get("iterateOn") {
        let collection = iterate_on
            .as_str()
            .ok_or(CommandExpansionError::ExpectedString)?;
        let placeholder = match collection {
            "profiles" => "profile.name",
            "schemes" => "scheme.name",
            _ => return Err(CommandExpansionError::ExpectedCollectionArray),
        };
        let mut result = Vec::new();
        for name in collection_names(root, collection)? {
            let mut nested_context = context.clone();
            nested_context.insert(placeholder.to_owned(), name);
            result.push(parse_expanded_entry(entry, root, &nested_context)?);
        }
        return Ok(result);
    }

    Ok(vec![parse_expanded_entry(entry, root, context)?])
}

fn parse_expanded_entry(
    entry: &JsonObject,
    root: &JsonObject,
    context: &BTreeMap<String, String>,
) -> Result<PaletteCommand, CommandExpansionError> {
    if let Some(commands) = entry.get("commands") {
        let commands = commands
            .as_array()
            .ok_or(CommandExpansionError::ExpectedCommandsArray)?;
        let mut nested = Vec::new();
        for command in commands {
            nested.extend(expand_entry(command, root, context)?);
        }
        return Ok(PaletteCommand {
            name: required_or_generated_name(entry, None, context)?,
            action: PaletteAction::Invalid,
            split_direction: None,
            profile: String::new(),
            commandline: String::new(),
            nested,
        });
    }

    let command = entry
        .get("command")
        .and_then(JsonValue::as_object)
        .ok_or(CommandExpansionError::ExpectedCommandObject)?;
    command_leaf(entry, command, context)
}

fn parse_template_entry(
    value: &JsonValue,
    context: &BTreeMap<String, String>,
) -> Result<PaletteCommand, CommandExpansionError> {
    let entry = value
        .as_object()
        .ok_or(CommandExpansionError::ExpectedEntryObject)?;
    if let Some(commands) = entry.get("commands") {
        let commands = commands
            .as_array()
            .ok_or(CommandExpansionError::ExpectedCommandsArray)?;
        let nested = commands
            .iter()
            .map(|command| parse_template_entry(command, context))
            .collect::<Result<Vec<_>, _>>()?;
        return Ok(PaletteCommand {
            name: required_or_generated_name(entry, None, context)?,
            action: PaletteAction::Invalid,
            split_direction: None,
            profile: String::new(),
            commandline: String::new(),
            nested,
        });
    }

    let command = entry
        .get("command")
        .and_then(JsonValue::as_object)
        .ok_or(CommandExpansionError::ExpectedCommandObject)?;
    command_leaf(entry, command, context)
}

fn command_leaf(
    entry: &JsonObject,
    command: &JsonObject,
    context: &BTreeMap<String, String>,
) -> Result<PaletteCommand, CommandExpansionError> {
    let action_name = command
        .get("action")
        .and_then(JsonValue::as_str)
        .ok_or(CommandExpansionError::ExpectedString)?;
    let action = match action_name {
        "splitPane" => PaletteAction::SplitPane,
        "newTab" => PaletteAction::NewTab,
        _ => PaletteAction::Invalid,
    };
    let profile = optional_substituted_string(command, "profile", context)?;
    let commandline = optional_substituted_string(command, "commandline", context)?;
    let split_direction = (action == PaletteAction::SplitPane).then(|| {
        match command.get("split").and_then(JsonValue::as_str) {
            Some("right" | "vertical") => PaletteSplitDirection::Right,
            Some("down" | "horizontal") => PaletteSplitDirection::Down,
            _ => PaletteSplitDirection::Automatic,
        }
    });

    Ok(PaletteCommand {
        name: required_or_generated_name(entry, Some(command), context)?,
        action,
        split_direction,
        profile,
        commandline,
        nested: Vec::new(),
    })
}

fn required_or_generated_name(
    entry: &JsonObject,
    command: Option<&JsonObject>,
    context: &BTreeMap<String, String>,
) -> Result<String, CommandExpansionError> {
    if let Some(name) = entry.get("name") {
        return name
            .as_str()
            .map(|name| substitute(name, context))
            .ok_or(CommandExpansionError::ExpectedName);
    }
    generated_name(command.ok_or(CommandExpansionError::ExpectedName)?, context)
}

fn generated_name(
    command: &JsonObject,
    context: &BTreeMap<String, String>,
) -> Result<String, CommandExpansionError> {
    let action = command
        .get("action")
        .and_then(JsonValue::as_str)
        .ok_or(CommandExpansionError::ExpectedString)?;
    let profile = optional_substituted_string(command, "profile", context)?;
    match action {
        "splitPane" => {
            let split = match command.get("split").and_then(JsonValue::as_str) {
                Some("right" | "vertical") => Some("right"),
                Some("down" | "horizontal") => Some("down"),
                _ => None,
            };
            Ok(match (split, profile.is_empty()) {
                (Some(split), false) => format!("Split pane, split: {split}, profile: {profile}"),
                (None, false) => format!("Split pane, profile: {profile}"),
                (Some(split), true) => format!("Split pane, split: {split}"),
                (None, true) => "Split pane".to_owned(),
            })
        }
        "newTab" if !profile.is_empty() => Ok(format!("New tab, profile: {profile}")),
        "newTab" => Ok("New tab".to_owned()),
        _ => Err(CommandExpansionError::ExpectedName),
    }
}

fn optional_substituted_string(
    object: &JsonObject,
    key: &str,
    context: &BTreeMap<String, String>,
) -> Result<String, CommandExpansionError> {
    match object.get(key) {
        None | Some(JsonValue::Null) => Ok(String::new()),
        Some(JsonValue::String(value)) => Ok(substitute(value, context)),
        Some(_) => Err(CommandExpansionError::ExpectedString),
    }
}

fn substitute(value: &str, context: &BTreeMap<String, String>) -> String {
    context
        .iter()
        .fold(value.to_owned(), |current, (key, value)| {
            current.replace(&format!("${{{key}}}"), value)
        })
}
