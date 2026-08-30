//! Portable profile/action elevation resolution for `TerminalApp` settings.
//!
//! The action argument is tri-state: omitted inherits the profile, while an
//! explicit `false` or `true` overrides it. This owner keeps that distinction
//! without depending on `WinRT` nullable wrappers.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonObject, JsonValue};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ElevateSettingsError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesArray,
    ExpectedProfileObject,
    ExpectedKeybindingsArray,
    ExpectedKeybindingObject,
    ExpectedCommandObject,
    ExpectedString,
    ExpectedBoolean,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ElevateBindingSnapshot {
    pub profile: String,
    pub action_elevate: Option<bool>,
    pub effective_elevate: bool,
    pub commandline: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ProfileElevation {
    commandline: String,
    elevate: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct BindingElevation {
    profile: String,
    elevate: Option<bool>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ElevateSettingsModel {
    profiles: BTreeMap<String, ProfileElevation>,
    bindings: BTreeMap<String, BindingElevation>,
}

impl ElevateSettingsModel {
    /// Parses the profile and keybinding fields that participate in elevation.
    ///
    /// # Errors
    ///
    /// Returns [`ElevateSettingsError`] for malformed profile, binding, string
    /// or boolean shapes used by this projection.
    pub fn from_json(input: &str) -> Result<Self, ElevateSettingsError> {
        let root = settings_json::parse(input).map_err(|_| ElevateSettingsError::InvalidJson)?;
        let root = root
            .as_object()
            .ok_or(ElevateSettingsError::ExpectedRootObject)?;
        Ok(Self {
            profiles: parse_profiles(root)?,
            bindings: parse_bindings(root)?,
        })
    }

    #[must_use]
    pub fn active_profile_count(&self) -> usize {
        self.profiles.len()
    }

    #[must_use]
    pub const fn warning_count(&self) -> usize {
        0
    }

    #[must_use]
    pub fn binding_count(&self) -> usize {
        self.bindings.len()
    }

    #[must_use]
    pub fn binding_snapshot(&self, key: &str) -> Option<ElevateBindingSnapshot> {
        let binding = self.bindings.get(&normalize_key(key))?;
        let profile = self.profiles.get(&binding.profile)?;
        Some(ElevateBindingSnapshot {
            profile: binding.profile.clone(),
            action_elevate: binding.elevate,
            effective_elevate: binding.elevate.unwrap_or(profile.elevate),
            commandline: profile.commandline.clone(),
        })
    }
}

fn parse_profiles(
    root: &JsonObject,
) -> Result<BTreeMap<String, ProfileElevation>, ElevateSettingsError> {
    let Some(profiles) = root.get("profiles") else {
        return Ok(BTreeMap::new());
    };
    let values = match profiles {
        JsonValue::Array(values) => values.as_slice(),
        JsonValue::Object(profiles) => match profiles.get("list") {
            Some(JsonValue::Array(values)) => values.as_slice(),
            None => &[],
            Some(_) => return Err(ElevateSettingsError::ExpectedProfilesArray),
        },
        _ => return Err(ElevateSettingsError::ExpectedProfilesArray),
    };

    let mut result = BTreeMap::new();
    for value in values {
        let profile = value
            .as_object()
            .ok_or(ElevateSettingsError::ExpectedProfileObject)?;
        let name = required_string(profile, "name")?.to_owned();
        let commandline = optional_string(profile, "commandline")?
            .unwrap_or_default()
            .to_owned();
        let elevate = optional_bool(profile, "elevate")?.unwrap_or(false);
        result.insert(
            name,
            ProfileElevation {
                commandline,
                elevate,
            },
        );
    }
    Ok(result)
}

fn parse_bindings(
    root: &JsonObject,
) -> Result<BTreeMap<String, BindingElevation>, ElevateSettingsError> {
    let Some(keybindings) = root.get("keybindings") else {
        return Ok(BTreeMap::new());
    };
    let JsonValue::Array(keybindings) = keybindings else {
        return Err(ElevateSettingsError::ExpectedKeybindingsArray);
    };

    let mut result = BTreeMap::new();
    for value in keybindings {
        let entry = value
            .as_object()
            .ok_or(ElevateSettingsError::ExpectedKeybindingObject)?;
        let command = entry
            .get("command")
            .and_then(JsonValue::as_object)
            .ok_or(ElevateSettingsError::ExpectedCommandObject)?;
        if command.get("action").and_then(JsonValue::as_str) != Some("newTab") {
            continue;
        }
        let profile = required_string(command, "profile")?.to_owned();
        let elevate = optional_bool(command, "elevate")?;
        for key in parse_keys(entry.get("keys"))? {
            result.insert(
                normalize_key(&key),
                BindingElevation {
                    profile: profile.clone(),
                    elevate,
                },
            );
        }
    }
    Ok(result)
}

fn parse_keys(value: Option<&JsonValue>) -> Result<Vec<String>, ElevateSettingsError> {
    match value {
        None => Ok(Vec::new()),
        Some(JsonValue::String(value)) => Ok(vec![value.clone()]),
        Some(JsonValue::Array(values)) => values
            .iter()
            .map(|value| {
                value
                    .as_str()
                    .map(str::to_owned)
                    .ok_or(ElevateSettingsError::ExpectedString)
            })
            .collect(),
        Some(_) => Err(ElevateSettingsError::ExpectedString),
    }
}

fn required_string<'a>(object: &'a JsonObject, key: &str) -> Result<&'a str, ElevateSettingsError> {
    object
        .get(key)
        .and_then(JsonValue::as_str)
        .ok_or(ElevateSettingsError::ExpectedString)
}

fn optional_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a str>, ElevateSettingsError> {
    match object.get(key) {
        None | Some(JsonValue::Null) => Ok(None),
        Some(JsonValue::String(value)) => Ok(Some(value)),
        Some(_) => Err(ElevateSettingsError::ExpectedString),
    }
}

fn optional_bool(object: &JsonObject, key: &str) -> Result<Option<bool>, ElevateSettingsError> {
    match object.get(key) {
        None | Some(JsonValue::Null) => Ok(None),
        Some(JsonValue::Bool(value)) => Ok(Some(*value)),
        Some(_) => Err(ElevateSettingsError::ExpectedBoolean),
    }
}

fn normalize_key(key: &str) -> String {
    key.trim().to_ascii_lowercase()
}
