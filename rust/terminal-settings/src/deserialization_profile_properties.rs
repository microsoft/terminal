//! Portable profile-property projection for `CascadiaSettings` deserialization.
//!
//! This seam composes the existing profile collection/order owner with
//! `profiles.defaults` and per-profile property precedence. It intentionally
//! owns only deterministic profile properties needed by Microsoft's
//! deserialization contracts; fragments, color-scheme validation and `WinRT`
//! projection remain separate seams.

use crate::{
    deserialization_profiles::{DeserializationProfileError, DeserializedProfiles},
    profile::ProfileGuid,
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

const DEFAULT_HISTORY_SIZE: i32 = 9001;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CloseOnExitMode {
    Graceful,
    Always,
    Never,
    Automatic,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeserializedProfileProperties {
    name: Option<String>,
    source: Option<String>,
    guid: Option<ProfileGuid>,
    history_size: i32,
    close_on_exit: CloseOnExitMode,
}

impl DeserializedProfileProperties {
    #[must_use]
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    #[must_use]
    pub fn source(&self) -> Option<&str> {
        self.source.as_deref()
    }

    #[must_use]
    pub const fn guid(&self) -> Option<ProfileGuid> {
        self.guid
    }

    #[must_use]
    pub const fn history_size(&self) -> i32 {
        self.history_size
    }

    #[must_use]
    pub const fn close_on_exit(&self) -> CloseOnExitMode {
        self.close_on_exit
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeserializedProfilePropertySet {
    profiles: Vec<DeserializedProfileProperties>,
}

impl DeserializedProfilePropertySet {
    /// Projects profile defaults/properties over the already-migrated
    /// user-first profile collection semantics.
    ///
    /// Microsoft layering precedence for the migrated property surface is:
    /// inbox/dynamic profile -> user `profiles.defaults` -> explicit user
    /// profile. Identity fields are never inherited from `profiles.defaults`.
    ///
    /// # Errors
    ///
    /// Returns [`DeserializationProfileError`] for malformed JSON/profile
    /// shapes or invalid migrated property values.
    pub fn from_user_and_inbox(
        user_json: &str,
        inbox_json: &str,
    ) -> Result<Self, DeserializationProfileError> {
        let ordered = DeserializedProfiles::from_user_and_inbox(user_json, inbox_json)?;
        let user_root = parse_root(user_json)?;
        let user_profiles = parse_profile_objects(&user_root)?;
        let defaults = profile_defaults(&user_root)?;

        let mut profiles = Vec::with_capacity(ordered.profile_count());
        for profile in ordered.profiles() {
            let object = profile.object();
            let identity = identity_key(object)?;
            let explicit_user = user_profiles
                .iter()
                .find(|candidate| identity_key(candidate).ok().as_ref() == Some(&identity));

            let explicit_history = explicit_user
                .map(|candidate| optional_i32(candidate, "historySize"))
                .transpose()?
                .flatten();
            let default_history = defaults
                .as_ref()
                .map(|value| optional_i32(value, "historySize"))
                .transpose()?
                .flatten();
            let history_size = if let Some(value) = explicit_history {
                value
            } else if let Some(value) = default_history {
                value
            } else {
                optional_i32(object, "historySize")?.unwrap_or(DEFAULT_HISTORY_SIZE)
            };

            profiles.push(DeserializedProfileProperties {
                name: optional_string(object, "name")?.map(ToOwned::to_owned),
                source: optional_string(object, "source")?.map(ToOwned::to_owned),
                guid: optional_guid(object)?,
                history_size,
                close_on_exit: parse_close_on_exit(object)?,
            });
        }

        Ok(Self { profiles })
    }

    #[must_use]
    pub fn profiles(&self) -> &[DeserializedProfileProperties] {
        &self.profiles
    }

    #[must_use]
    pub fn profile_by_name(&self, name: &str) -> Option<&DeserializedProfileProperties> {
        self.profiles
            .iter()
            .find(|profile| profile.name() == Some(name))
    }

    #[must_use]
    pub fn profile_by_guid(&self, guid: ProfileGuid) -> Option<&DeserializedProfileProperties> {
        self.profiles
            .iter()
            .find(|profile| profile.guid() == Some(guid))
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum IdentityKey {
    Guid(ProfileGuid),
    Generated {
        name: String,
        source: Option<String>,
    },
}

fn identity_key(object: &JsonObject) -> Result<IdentityKey, DeserializationProfileError> {
    if let Some(guid) = optional_guid(object)? {
        return Ok(IdentityKey::Guid(guid));
    }
    Ok(IdentityKey::Generated {
        name: optional_string(object, "name")?
            .unwrap_or_default()
            .to_owned(),
        source: optional_string(object, "source")?.map(ToOwned::to_owned),
    })
}

fn parse_root(input: &str) -> Result<JsonObject, DeserializationProfileError> {
    let value =
        settings_json::parse(input).map_err(|_| DeserializationProfileError::InvalidJson)?;
    value
        .as_object()
        .cloned()
        .ok_or(DeserializationProfileError::ExpectedRootObject)
}

fn parse_profile_objects(
    root: &JsonObject,
) -> Result<Vec<JsonObject>, DeserializationProfileError> {
    match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => clone_objects(values),
        JsonMember::Value(JsonValue::Object(profiles)) => {
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
                JsonMember::Value(JsonValue::Array(values)) => clone_objects(values),
                JsonMember::Value(_) => Err(DeserializationProfileError::ExpectedProfilesArray),
            }
        }
        JsonMember::Value(_) => Err(DeserializationProfileError::ExpectedProfilesArray),
    }
}

fn clone_objects(values: &[JsonValue]) -> Result<Vec<JsonObject>, DeserializationProfileError> {
    values
        .iter()
        .map(|value| {
            value
                .as_object()
                .cloned()
                .ok_or(DeserializationProfileError::ExpectedProfileObject)
        })
        .collect()
}

fn profile_defaults(root: &JsonObject) -> Result<Option<JsonObject>, DeserializationProfileError> {
    let JsonMember::Value(JsonValue::Object(profiles)) = JsonMember::from_object(root, "profiles")
    else {
        return Ok(None);
    };
    match JsonMember::from_object(profiles, "defaults") {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::Object(defaults)) => Ok(Some(defaults.clone())),
        JsonMember::Value(_) => Err(DeserializationProfileError::ExpectedProfileObject),
    }
}

fn optional_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a str>, DeserializationProfileError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => Ok(Some(value.as_str())),
        JsonMember::Value(_) => Err(DeserializationProfileError::InvalidString),
    }
}

fn optional_guid(object: &JsonObject) -> Result<Option<ProfileGuid>, DeserializationProfileError> {
    match JsonMember::from_object(object, "guid") {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => ProfileGuid::parse(value)
            .map(Some)
            .map_err(|_| DeserializationProfileError::InvalidGuid),
        JsonMember::Value(_) => Err(DeserializationProfileError::InvalidGuid),
    }
}

fn optional_i32(
    object: &JsonObject,
    key: &str,
) -> Result<Option<i32>, DeserializationProfileError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::Number(value)) => {
            if value.fract() != 0.0 || *value < f64::from(i32::MIN) || *value > f64::from(i32::MAX)
            {
                return Err(DeserializationProfileError::InvalidInteger);
            }
            Ok(Some(*value as i32))
        }
        JsonMember::Value(_) => Err(DeserializationProfileError::InvalidInteger),
    }
}

fn parse_close_on_exit(
    object: &JsonObject,
) -> Result<CloseOnExitMode, DeserializationProfileError> {
    match JsonMember::from_object(object, "closeOnExit") {
        JsonMember::Missing | JsonMember::Null => Ok(CloseOnExitMode::Automatic),
        JsonMember::Value(JsonValue::Bool(true)) => Ok(CloseOnExitMode::Graceful),
        JsonMember::Value(JsonValue::Bool(false)) => Ok(CloseOnExitMode::Never),
        JsonMember::Value(JsonValue::String(value)) => Ok(match value.as_str() {
            "graceful" => CloseOnExitMode::Graceful,
            "always" => CloseOnExitMode::Always,
            "never" => CloseOnExitMode::Never,
            "automatic" => CloseOnExitMode::Automatic,
            _ => CloseOnExitMode::Automatic,
        }),
        JsonMember::Value(_) => Err(DeserializationProfileError::InvalidString),
    }
}
