//! Portable `CascadiaSettings` deserialization validation.
//!
//! This owner centralizes validation that spans the already-migrated profile and
//! action JSON surfaces: color-scheme references plus ActionMap/keybinding
//! argument warnings. It deliberately does not own global-property clamping or
//! permissive JSON syntax, which remain separate seams.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonMember, JsonObject, JsonValue};

const DEFAULT_COLOR_SCHEME: &str = "Campbell";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationValidationWarning {
    UnknownColorScheme,
    InvalidColorSchemeInCmd,
    AtLeastOneKeybindingWarning,
    TooManyKeysForChord,
    MissingRequiredParameter,
    FailedToParseSubCommands,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationValidationError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedArray,
    ExpectedObject,
    InvalidString,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct DeserializationValidation {
    profile_schemes: Vec<(String, String)>,
    key_map: BTreeMap<String, bool>,
    action_map_count: usize,
    name_map_count: usize,
    keybinding_warnings: Vec<DeserializationValidationWarning>,
    settings_warnings: Vec<DeserializationValidationWarning>,
}

impl DeserializationValidation {
    /// Validates the portable profile/action/keybinding surfaces of one user
    /// settings document against the schemes supplied by the lower/inbox layer.
    ///
    /// # Errors
    ///
    /// Returns [`DeserializationValidationError`] for malformed migrated JSON
    /// container shapes or string fields.
    pub fn from_user_and_inbox(
        user_json: &str,
        inbox_json: &str,
    ) -> Result<Self, DeserializationValidationError> {
        let user_root = parse_root(user_json)?;
        let inbox_root = parse_root(inbox_json)?;
        let known_schemes = known_scheme_names(&user_root, &inbox_root)?;
        let mut validation = Self::default();

        validation.validate_profile_schemes(&user_root, &known_schemes)?;
        validation.validate_command_schemes(&user_root, &known_schemes)?;
        validation.validate_keybindings(&user_root)?;
        Ok(validation)
    }

    #[must_use]
    pub fn profile_scheme(&self, index: usize) -> Option<(&str, &str)> {
        self.profile_schemes
            .get(index)
            .map(|(dark, light)| (dark.as_str(), light.as_str()))
    }

    #[must_use]
    pub fn key_map_count(&self) -> usize {
        self.key_map.len()
    }

    #[must_use]
    pub const fn action_map_count(&self) -> usize {
        self.action_map_count
    }

    #[must_use]
    pub const fn name_map_count(&self) -> usize {
        self.name_map_count
    }

    #[must_use]
    pub fn action_is_bound_for_key(&self, key: &str) -> bool {
        self.key_map
            .get(&normalize_key(key))
            .copied()
            .unwrap_or(false)
    }

    #[must_use]
    pub fn keybinding_warnings(&self) -> &[DeserializationValidationWarning] {
        &self.keybinding_warnings
    }

    #[must_use]
    pub fn settings_warnings(&self) -> &[DeserializationValidationWarning] {
        &self.settings_warnings
    }

    fn validate_profile_schemes(
        &mut self,
        root: &JsonObject,
        known_schemes: &[String],
    ) -> Result<(), DeserializationValidationError> {
        let profiles = profile_values(root)?;
        for profile in profiles {
            let object = profile
                .as_object()
                .ok_or(DeserializationValidationError::ExpectedObject)?;
            let (dark, light) = color_scheme_names(object)?;
            let dark_valid = known_schemes.iter().any(|known| known == dark);
            let light_valid = known_schemes.iter().any(|known| known == light);
            if !dark_valid || !light_valid {
                push_unique(
                    &mut self.settings_warnings,
                    DeserializationValidationWarning::UnknownColorScheme,
                );
            }
            self.profile_schemes.push((
                if dark_valid {
                    dark
                } else {
                    DEFAULT_COLOR_SCHEME
                }
                .to_owned(),
                if light_valid {
                    light
                } else {
                    DEFAULT_COLOR_SCHEME
                }
                .to_owned(),
            ));
        }
        Ok(())
    }

    fn validate_command_schemes(
        &mut self,
        root: &JsonObject,
        known_schemes: &[String],
    ) -> Result<(), DeserializationValidationError> {
        let Some(actions) = root.get("actions") else {
            return Ok(());
        };
        let JsonValue::Array(actions) = actions else {
            return Err(DeserializationValidationError::ExpectedArray);
        };
        if actions_have_invalid_scheme(actions, known_schemes)? {
            push_unique(
                &mut self.settings_warnings,
                DeserializationValidationWarning::InvalidColorSchemeInCmd,
            );
        }
        Ok(())
    }

    fn validate_keybindings(
        &mut self,
        root: &JsonObject,
    ) -> Result<(), DeserializationValidationError> {
        let Some(keybindings) = root.get("keybindings") else {
            return Ok(());
        };
        let JsonValue::Array(keybindings) = keybindings else {
            return Err(DeserializationValidationError::ExpectedArray);
        };

        for entry in keybindings {
            let object = entry
                .as_object()
                .ok_or(DeserializationValidationError::ExpectedObject)?;

            if let Some(commands) = object.get("commands") {
                let valid = nested_commands_are_valid(commands)?;
                if !valid {
                    self.keybinding_warnings
                        .push(DeserializationValidationWarning::FailedToParseSubCommands);
                }
                continue;
            }

            let parsed_keys = parse_keys(object.get("keys"))?;
            let too_many_keys = parsed_keys.len() > 1;
            if too_many_keys {
                self.keybinding_warnings
                    .push(DeserializationValidationWarning::TooManyKeysForChord);
            }

            let command = object.get("command");
            let command_valid = match command {
                Some(command) => validate_required_parameters(command),
                None => true,
            };
            if !command_valid {
                self.keybinding_warnings
                    .push(DeserializationValidationWarning::MissingRequiredParameter);
            }

            if command_valid {
                if command.is_some() {
                    self.action_map_count += 1;
                    self.name_map_count += 1;
                }
                if !too_many_keys {
                    for key in parsed_keys {
                        self.key_map.insert(key, true);
                    }
                }
            } else if !too_many_keys {
                for key in parsed_keys {
                    self.key_map.insert(key, false);
                }
            }
        }

        if !self.keybinding_warnings.is_empty() {
            self.settings_warnings
                .push(DeserializationValidationWarning::AtLeastOneKeybindingWarning);
            self.settings_warnings
                .extend(self.keybinding_warnings.iter().copied());
        }
        Ok(())
    }
}

fn parse_root(input: &str) -> Result<JsonObject, DeserializationValidationError> {
    let value =
        settings_json::parse(input).map_err(|_| DeserializationValidationError::InvalidJson)?;
    value
        .as_object()
        .cloned()
        .ok_or(DeserializationValidationError::ExpectedRootObject)
}

fn profile_values(root: &JsonObject) -> Result<&[JsonValue], DeserializationValidationError> {
    match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => Ok(&[]),
        JsonMember::Value(JsonValue::Array(values)) => Ok(values),
        JsonMember::Value(JsonValue::Object(profiles)) => {
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => Ok(&[]),
                JsonMember::Value(JsonValue::Array(values)) => Ok(values),
                JsonMember::Value(_) => Err(DeserializationValidationError::ExpectedArray),
            }
        }
        JsonMember::Value(_) => Err(DeserializationValidationError::ExpectedArray),
    }
}

fn known_scheme_names(
    user_root: &JsonObject,
    inbox_root: &JsonObject,
) -> Result<Vec<String>, DeserializationValidationError> {
    let mut names = Vec::new();
    collect_scheme_names(inbox_root, &mut names)?;
    collect_scheme_names(user_root, &mut names)?;
    if !names.iter().any(|name| name == DEFAULT_COLOR_SCHEME) {
        names.push(DEFAULT_COLOR_SCHEME.to_owned());
    }
    Ok(names)
}

fn collect_scheme_names(
    root: &JsonObject,
    names: &mut Vec<String>,
) -> Result<(), DeserializationValidationError> {
    let Some(schemes) = root.get("schemes") else {
        return Ok(());
    };
    let JsonValue::Array(schemes) = schemes else {
        return Err(DeserializationValidationError::ExpectedArray);
    };
    for scheme in schemes {
        let object = scheme
            .as_object()
            .ok_or(DeserializationValidationError::ExpectedObject)?;
        if let Some(name) = optional_string(object, "name")?
            && !names.iter().any(|known| known == name)
        {
            names.push(name.to_owned());
        }
    }
    Ok(())
}

fn color_scheme_names(
    profile: &JsonObject,
) -> Result<(&str, &str), DeserializationValidationError> {
    match JsonMember::from_object(profile, "colorScheme") {
        JsonMember::Missing | JsonMember::Null => Ok((DEFAULT_COLOR_SCHEME, DEFAULT_COLOR_SCHEME)),
        JsonMember::Value(JsonValue::String(value)) => Ok((value.as_str(), value.as_str())),
        JsonMember::Value(JsonValue::Object(value)) => Ok((
            optional_string(value, "dark")?.unwrap_or(DEFAULT_COLOR_SCHEME),
            optional_string(value, "light")?.unwrap_or(DEFAULT_COLOR_SCHEME),
        )),
        JsonMember::Value(_) => Err(DeserializationValidationError::InvalidString),
    }
}

fn actions_have_invalid_scheme(
    entries: &[JsonValue],
    known_schemes: &[String],
) -> Result<bool, DeserializationValidationError> {
    for entry in entries {
        let object = entry
            .as_object()
            .ok_or(DeserializationValidationError::ExpectedObject)?;
        if let Some(command) = object.get("command")
            && command_has_invalid_scheme(command, known_schemes)?
        {
            return Ok(true);
        }
        if let Some(commands) = object.get("commands") {
            let JsonValue::Array(commands) = commands else {
                return Err(DeserializationValidationError::ExpectedArray);
            };
            if actions_have_invalid_scheme(commands, known_schemes)? {
                return Ok(true);
            }
        }
    }
    Ok(false)
}

fn command_has_invalid_scheme(
    command: &JsonValue,
    known_schemes: &[String],
) -> Result<bool, DeserializationValidationError> {
    let JsonValue::Object(command) = command else {
        return Ok(false);
    };
    if command.get("action").and_then(JsonValue::as_str) != Some("setColorScheme") {
        return Ok(false);
    }
    let Some(scheme) = command.get("colorScheme") else {
        return Ok(false);
    };
    let Some(scheme) = scheme.as_str() else {
        return Err(DeserializationValidationError::InvalidString);
    };
    Ok(!known_schemes.iter().any(|known| known == scheme))
}

fn nested_commands_are_valid(commands: &JsonValue) -> Result<bool, DeserializationValidationError> {
    let JsonValue::Array(commands) = commands else {
        return Ok(false);
    };
    for command in commands {
        let object = command
            .as_object()
            .ok_or(DeserializationValidationError::ExpectedObject)?;
        if object.get("command").is_none() && object.get("commands").is_none() {
            return Ok(false);
        }
    }
    Ok(true)
}

fn parse_keys(value: Option<&JsonValue>) -> Result<Vec<String>, DeserializationValidationError> {
    match value {
        None | Some(JsonValue::Null) => Ok(Vec::new()),
        Some(JsonValue::String(value)) => Ok(vec![normalize_key(value)]),
        Some(JsonValue::Array(values)) => values
            .iter()
            .map(|value| {
                value
                    .as_str()
                    .map(normalize_key)
                    .ok_or(DeserializationValidationError::InvalidString)
            })
            .collect(),
        Some(_) => Err(DeserializationValidationError::InvalidString),
    }
}

fn validate_required_parameters(command: &JsonValue) -> bool {
    let JsonValue::Object(command) = command else {
        return true;
    };
    match command.get("action").and_then(JsonValue::as_str) {
        Some("moveFocus" | "resizePane") => command
            .get("direction")
            .and_then(JsonValue::as_str)
            .is_some_and(|value| !value.is_empty()),
        Some("wt") => command
            .get("commandline")
            .and_then(JsonValue::as_str)
            .is_some_and(|value| !value.is_empty()),
        _ => true,
    }
}

fn optional_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a str>, DeserializationValidationError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => Ok(Some(value.as_str())),
        JsonMember::Value(_) => Err(DeserializationValidationError::InvalidString),
    }
}

fn push_unique(
    warnings: &mut Vec<DeserializationValidationWarning>,
    warning: DeserializationValidationWarning,
) {
    if !warnings.contains(&warning) {
        warnings.push(warning);
    }
}

fn normalize_key(key: &str) -> String {
    key.trim().to_ascii_lowercase()
}
