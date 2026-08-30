//! Portable profile-collection layering, fixup and validation semantics from `SettingsModel`.
//!
//! This owner keeps legacy top-level `profiles` arrays as full JSON objects,
//! layers user objects over inbox objects by strict profile GUID identity,
//! preserves source order for profiles that are not replaced, owns the
//! deterministic legacy cmd/PowerShell commandline fixups applied by the
//! settings loader, and records the migrated profile-validation warnings.

use crate::{
    profile::{ProfileGuid, ProfileParseError},
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

const DEFAULT_WINDOWS_POWERSHELL_GUID: &str = "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";
const DEFAULT_COMMAND_PROMPT_GUID: &str = "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}";
const LEGACY_POWERSHELL_COMMANDLINE: &str = "powershell.exe";
const CANONICAL_POWERSHELL_COMMANDLINE: &str =
    "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
const LEGACY_COMMAND_PROMPT_COMMANDLINE: &str = "cmd.exe";
const CANONICAL_COMMAND_PROMPT_COMMANDLINE: &str = "%SystemRoot%\\System32\\cmd.exe";
const DEFAULT_COLOR_SCHEME_NAME: &str = "Campbell";

/// Profile-related settings warnings currently owned by safe Rust.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SettingsLoadWarning {
    MissingDefaultProfile,
    UnknownColorScheme,
    InvalidProfileEnvironmentVariables,
}

/// One layered profile object together with the identity fields needed by the
/// settings loader to reconcile inbox and user entries.
#[derive(Debug, Clone, PartialEq)]
pub struct LayeredProfile {
    object: JsonObject,
    name: Option<String>,
    guid: Option<ProfileGuid>,
}

impl LayeredProfile {
    fn from_object(object: JsonObject) -> Result<Self, ProfileParseError> {
        let mut profile = Self {
            object,
            name: None,
            guid: None,
        };
        profile.refresh_identity()?;
        Ok(profile)
    }

    fn layer_object(&mut self, overlay: JsonObject) -> Result<(), ProfileParseError> {
        for (key, value) in overlay {
            self.object.insert(key, value);
        }
        self.refresh_identity()
    }

    fn refresh_identity(&mut self) -> Result<(), ProfileParseError> {
        self.name = match JsonMember::from_object(&self.object, "name") {
            JsonMember::Missing | JsonMember::Null => None,
            JsonMember::Value(JsonValue::String(value)) => Some(value.clone()),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
        };
        self.guid = match JsonMember::from_object(&self.object, "guid") {
            JsonMember::Missing | JsonMember::Null => None,
            JsonMember::Value(JsonValue::String(value)) => Some(ProfileGuid::parse(value)?),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidGuid),
        };
        Ok(())
    }

    fn apply_legacy_shell_commandline_fixup(
        &mut self,
        powershell_guid: ProfileGuid,
        command_prompt_guid: ProfileGuid,
    ) {
        let Some(guid) = self.guid else {
            return;
        };
        let commandline = match JsonMember::from_object(&self.object, "commandline") {
            JsonMember::Value(JsonValue::String(value)) => value.as_str(),
            _ => return,
        };

        let replacement = if guid == powershell_guid
            && commandline.eq_ignore_ascii_case(LEGACY_POWERSHELL_COMMANDLINE)
        {
            Some(CANONICAL_POWERSHELL_COMMANDLINE)
        } else if guid == command_prompt_guid
            && commandline.eq_ignore_ascii_case(LEGACY_COMMAND_PROMPT_COMMANDLINE)
        {
            Some(CANONICAL_COMMAND_PROMPT_COMMANDLINE)
        } else {
            None
        };

        if let Some(replacement) = replacement {
            self.object.insert(
                "commandline".to_owned(),
                JsonValue::String(replacement.to_owned()),
            );
        }
    }

    #[must_use]
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    #[must_use]
    pub const fn guid(&self) -> Option<ProfileGuid> {
        self.guid
    }

    #[must_use]
    pub fn commandline(&self) -> Option<&str> {
        match JsonMember::from_object(&self.object, "commandline") {
            JsonMember::Value(JsonValue::String(value)) => Some(value.as_str()),
            _ => None,
        }
    }

    /// Returns the fully layered JSON object, including properties that are not
    /// yet projected into the portable `Profile` owner.
    #[must_use]
    pub const fn object(&self) -> &JsonObject {
        &self.object
    }
}

/// Safe Rust owner for profile collection reconciliation, deterministic fixups
/// and the currently migrated profile-validation warnings.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct ProfileCollection {
    profiles: Vec<LayeredProfile>,
    warnings: Vec<SettingsLoadWarning>,
}

impl ProfileCollection {
    /// Layers user `profiles` entries over inbox entries with the same GUID.
    /// Matching user entries retain the inbox position, unmatched inbox entries
    /// remain present, and unmatched user entries append in user order.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when either settings document is malformed,
    /// `profiles` is not an array, a profile is not an object, or an identity
    /// field has an invalid type/value.
    pub fn from_layered_legacy_arrays(
        user_json: &str,
        inbox_json: &str,
    ) -> Result<Self, ProfileParseError> {
        let mut profiles = parse_legacy_profile_objects(inbox_json)?
            .into_iter()
            .map(LayeredProfile::from_object)
            .collect::<Result<Vec<_>, _>>()?;

        for object in parse_legacy_profile_objects(user_json)? {
            let incoming = LayeredProfile::from_object(object.clone())?;
            let matching_index = incoming.guid().and_then(|guid| {
                profiles
                    .iter()
                    .position(|profile| profile.guid() == Some(guid))
            });

            if let Some(index) = matching_index {
                profiles[index].layer_object(object)?;
            } else {
                profiles.push(incoming);
            }
        }

        Ok(Self {
            profiles,
            warnings: Vec::new(),
        })
    }

    /// Parses a modern `profiles.list` user layer and applies the deterministic
    /// commandline compatibility patches used by Microsoft's `FixupUserSettings`.
    /// Only the canonical Windows PowerShell and Command Prompt GUIDs are
    /// eligible, and the old executable names are matched ASCII-case-insensitively.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the settings/profile structure or a
    /// profile identity is invalid.
    pub fn from_user_json_with_legacy_shell_path_fixups(
        user_json: &str,
    ) -> Result<Self, ProfileParseError> {
        let powershell_guid = ProfileGuid::parse(DEFAULT_WINDOWS_POWERSHELL_GUID)?;
        let command_prompt_guid = ProfileGuid::parse(DEFAULT_COMMAND_PROMPT_GUID)?;
        let mut profiles = parse_modern_profile_objects(user_json)?
            .into_iter()
            .map(LayeredProfile::from_object)
            .collect::<Result<Vec<_>, _>>()?;

        for profile in &mut profiles {
            profile.apply_legacy_shell_commandline_fixup(powershell_guid, command_prompt_guid);
        }

        Ok(Self {
            profiles,
            warnings: Vec::new(),
        })
    }

    /// Parses the user profile collection and runs the deterministic validation
    /// slice currently migrated from `CascadiaSettings`.
    ///
    /// Warning order follows the Microsoft constructor pipeline: an explicitly
    /// requested but unresolved default profile is handled first, color schemes
    /// are validated before the rest of `_validateSettings`, and profile
    /// environment names are checked afterward.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the settings/profile structure or a
    /// migrated validation input has an invalid shape or type.
    pub fn from_user_json_with_profile_validation(
        user_json: &str,
    ) -> Result<Self, ProfileParseError> {
        let value = settings_json::parse(user_json).map_err(|_| ProfileParseError::InvalidJson)?;
        let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
        let profiles = parse_profile_objects_from_root(root)?
            .into_iter()
            .map(LayeredProfile::from_object)
            .collect::<Result<Vec<_>, _>>()?;

        let mut warnings = Vec::new();
        if requested_default_profile_is_missing(root, &profiles)? {
            warnings.push(SettingsLoadWarning::MissingDefaultProfile);
        }

        if profiles_have_unknown_color_scheme(root, &profiles)? {
            warnings.push(SettingsLoadWarning::UnknownColorScheme);
        }

        for profile in &profiles {
            if profile_has_environment_name_collision(profile)? {
                warnings.push(SettingsLoadWarning::InvalidProfileEnvironmentVariables);
                break;
            }
        }

        Ok(Self { profiles, warnings })
    }

    #[must_use]
    pub fn profiles(&self) -> &[LayeredProfile] {
        &self.profiles
    }

    #[must_use]
    pub fn warnings(&self) -> &[SettingsLoadWarning] {
        &self.warnings
    }
}

fn requested_default_profile_is_missing(
    root: &JsonObject,
    profiles: &[LayeredProfile],
) -> Result<bool, ProfileParseError> {
    let requested = match JsonMember::from_object(root, "defaultProfile") {
        JsonMember::Missing | JsonMember::Null => return Ok(false),
        JsonMember::Value(JsonValue::String(value)) if value.is_empty() => return Ok(false),
        JsonMember::Value(JsonValue::String(value)) => value.as_str(),
        JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
    };

    if let Ok(guid) = ProfileGuid::parse(requested)
        && profiles.iter().any(|profile| profile.guid() == Some(guid))
    {
        return Ok(false);
    }

    Ok(!profiles
        .iter()
        .any(|profile| profile.name() == Some(requested)))
}

fn profiles_have_unknown_color_scheme(
    root: &JsonObject,
    profiles: &[LayeredProfile],
) -> Result<bool, ProfileParseError> {
    let known_schemes = known_color_scheme_names(root)?;
    for profile in profiles {
        let (dark, light) = profile_color_scheme_names(profile)?;
        if !known_schemes.contains(&dark) || !known_schemes.contains(&light) {
            return Ok(true);
        }
    }
    Ok(false)
}

fn known_color_scheme_names(root: &JsonObject) -> Result<Vec<&str>, ProfileParseError> {
    let schemes = match JsonMember::from_object(root, "schemes") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(schemes)) => schemes,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
    };

    let mut names = Vec::with_capacity(schemes.len());
    for scheme in schemes {
        let object = scheme
            .as_object()
            .ok_or(ProfileParseError::ExpectedObject)?;
        if let JsonMember::Value(JsonValue::String(name)) = JsonMember::from_object(object, "name")
        {
            names.push(name.as_str());
        }
    }
    Ok(names)
}

fn profile_color_scheme_names(profile: &LayeredProfile) -> Result<(&str, &str), ProfileParseError> {
    match JsonMember::from_object(profile.object(), "colorScheme") {
        JsonMember::Missing | JsonMember::Null => {
            Ok((DEFAULT_COLOR_SCHEME_NAME, DEFAULT_COLOR_SCHEME_NAME))
        }
        JsonMember::Value(JsonValue::String(value)) => Ok((value.as_str(), value.as_str())),
        JsonMember::Value(JsonValue::Object(value)) => {
            let dark = match JsonMember::from_object(value, "dark") {
                JsonMember::Missing | JsonMember::Null => DEFAULT_COLOR_SCHEME_NAME,
                JsonMember::Value(JsonValue::String(value)) => value.as_str(),
                JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
            };
            let light = match JsonMember::from_object(value, "light") {
                JsonMember::Missing | JsonMember::Null => DEFAULT_COLOR_SCHEME_NAME,
                JsonMember::Value(JsonValue::String(value)) => value.as_str(),
                JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
            };
            Ok((dark, light))
        }
        JsonMember::Value(_) => Err(ProfileParseError::InvalidString),
    }
}

fn profile_has_environment_name_collision(
    profile: &LayeredProfile,
) -> Result<bool, ProfileParseError> {
    let environment = match JsonMember::from_object(profile.object(), "environment") {
        JsonMember::Missing | JsonMember::Null => return Ok(false),
        JsonMember::Value(JsonValue::Object(environment)) => environment,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
    };

    let mut names: Vec<&str> = Vec::with_capacity(environment.len());
    for name in environment.keys() {
        // Microsoft's direct source vector uses ASCII environment names. This
        // reproduces the exact FOO/Foo ordinal-insensitive collision without
        // introducing locale-sensitive comparison into the portable owner.
        if names
            .iter()
            .any(|existing| existing.eq_ignore_ascii_case(name))
        {
            return Ok(true);
        }
        names.push(name.as_str());
    }
    Ok(false)
}

fn parse_legacy_profile_objects(input: &str) -> Result<Vec<JsonObject>, ProfileParseError> {
    let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
    let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
    let values = match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => values,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
    };

    clone_profile_objects(values)
}

fn parse_modern_profile_objects(input: &str) -> Result<Vec<JsonObject>, ProfileParseError> {
    let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
    let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
    let profiles = match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Object(profiles)) => profiles,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
    };
    let values = match JsonMember::from_object(profiles, "list") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => values,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
    };

    clone_profile_objects(values)
}

fn parse_profile_objects_from_root(
    root: &JsonObject,
) -> Result<Vec<JsonObject>, ProfileParseError> {
    match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => clone_profile_objects(values),
        JsonMember::Value(JsonValue::Object(profiles)) => {
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
                JsonMember::Value(JsonValue::Array(values)) => clone_profile_objects(values),
                JsonMember::Value(_) => Err(ProfileParseError::ExpectedArray),
            }
        }
        JsonMember::Value(_) => Err(ProfileParseError::ExpectedArray),
    }
}

fn clone_profile_objects(values: &[JsonValue]) -> Result<Vec<JsonObject>, ProfileParseError> {
    values
        .iter()
        .map(|value| {
            value
                .as_object()
                .cloned()
                .ok_or(ProfileParseError::ExpectedObject)
        })
        .collect()
}
