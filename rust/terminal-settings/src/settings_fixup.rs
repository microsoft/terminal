//! Portable `SettingsLoader::FixupUserSettings` semantics that operate on the
//! existing [`SettingsDocument`] serialization owner.

use crate::{
    serialization::{SerializationError, SettingsDocument},
    settings_json::JsonValue,
};

const CMD_GUID: &str = "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}";
const POWERSHELL_GUID: &str = "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";
const CMD_FULL_PATH: &str = "%SystemRoot%\\System32\\cmd.exe";
const POWERSHELL_FULL_PATH: &str =
    "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";

/// Applies the portable profile-commandline patching portion of Microsoft's
/// `SettingsLoader::FixupUserSettings`.
///
/// Only the two built-in profile GUIDs are patched, and only when the user
/// explicitly wrote the legacy short executable name. Inherited inbox values
/// are not materialized into the user's settings file.
///
/// # Errors
///
/// Returns [`SerializationError`] when the settings/profile shape is invalid.
pub fn fixup_user_settings(document: &mut SettingsDocument) -> Result<bool, SerializationError> {
    let patches = collect_commandline_patches(document)?;
    for (index, value) in &patches {
        document.set_profile_string(*index, "commandline", value)?;
    }
    Ok(!patches.is_empty())
}

/// Returns the effective commandline for a profile after inbox fallback.
///
/// This mirrors the observable used by Microsoft's commandline-patching tests:
/// an omitted commandline on the built-in Command Prompt or Windows PowerShell
/// profile resolves to its inbox full path without requiring a user-file fixup.
///
/// # Errors
///
/// Returns [`SerializationError`] when the settings/profile shape is invalid or
/// the requested profile does not exist.
pub fn effective_profile_commandline(
    document: &SettingsDocument,
    index: usize,
) -> Result<Option<String>, SerializationError> {
    let profiles = profile_values(document)?;
    let profile = profiles
        .get(index)
        .ok_or(SerializationError::ProfileNotFound)?;
    let profile = profile
        .as_object()
        .ok_or(SerializationError::ExpectedProfileObject)?;

    if let Some(commandline) = profile.get("commandline").and_then(JsonValue::as_str) {
        return Ok(Some(commandline.to_owned()));
    }

    let guid = profile.get("guid").and_then(JsonValue::as_str);
    Ok(match guid {
        Some(guid) if guid.eq_ignore_ascii_case(CMD_GUID) => Some(CMD_FULL_PATH.to_owned()),
        Some(guid) if guid.eq_ignore_ascii_case(POWERSHELL_GUID) => {
            Some(POWERSHELL_FULL_PATH.to_owned())
        }
        _ => None,
    })
}

fn collect_commandline_patches(
    document: &SettingsDocument,
) -> Result<Vec<(usize, &'static str)>, SerializationError> {
    let profiles = profile_values(document)?;
    let mut patches = Vec::new();

    for (index, profile) in profiles.iter().enumerate() {
        let profile = profile
            .as_object()
            .ok_or(SerializationError::ExpectedProfileObject)?;
        let Some(guid) = profile.get("guid").and_then(JsonValue::as_str) else {
            continue;
        };
        let Some(commandline) = profile.get("commandline").and_then(JsonValue::as_str) else {
            continue;
        };

        if guid.eq_ignore_ascii_case(CMD_GUID) && commandline.eq_ignore_ascii_case("cmd.exe") {
            patches.push((index, CMD_FULL_PATH));
        } else if guid.eq_ignore_ascii_case(POWERSHELL_GUID)
            && commandline.eq_ignore_ascii_case("powershell.exe")
        {
            patches.push((index, POWERSHELL_FULL_PATH));
        }
    }

    Ok(patches)
}

fn profile_values(document: &SettingsDocument) -> Result<&[JsonValue], SerializationError> {
    let root = document
        .to_json_value()
        .as_object()
        .ok_or(SerializationError::ExpectedRootObject)?;
    let profiles = root
        .get("profiles")
        .ok_or(SerializationError::ExpectedProfilesArray)?;

    match profiles {
        JsonValue::Array(profiles) => Ok(profiles.as_slice()),
        JsonValue::Object(profiles) => profiles
            .get("list")
            .and_then(JsonValue::as_array)
            .ok_or(SerializationError::ExpectedProfilesArray),
        _ => Err(SerializationError::ExpectedProfilesArray),
    }
}
