//! Portable `CascadiaSettings` profile deserialization aggregate.
//!
//! This owner models the profile-collection behavior shared by Microsoft's
//! deserialization validation, ordering, hiding and default-profile contracts.
//! It intentionally stops before color-scheme fallback, fragments and the
//! broader profile property surface, which are separate seams.

use std::collections::BTreeSet;

use crate::{
    profile::ProfileGuid,
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationProfileWarning {
    DuplicateProfile,
    MissingDefaultProfile,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationProfileError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesArray,
    ExpectedProfileObject,
    InvalidString,
    InvalidBoolean,
    InvalidInteger,
    InvalidGuid,
    NoProfiles,
    AllProfilesHidden,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
enum ProfileIdentity {
    Guid(ProfileGuid),
    Generated {
        name: String,
        source: Option<String>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub struct DeserializedProfile {
    object: JsonObject,
    identity: ProfileIdentity,
    hidden: bool,
}

impl DeserializedProfile {
    fn from_object(object: JsonObject) -> Result<Self, DeserializationProfileError> {
        let identity = profile_identity(&object)?;
        let hidden = match JsonMember::from_object(&object, "hidden") {
            JsonMember::Missing | JsonMember::Null => false,
            JsonMember::Value(JsonValue::Bool(value)) => *value,
            JsonMember::Value(_) => return Err(DeserializationProfileError::InvalidBoolean),
        };
        Ok(Self {
            object,
            identity,
            hidden,
        })
    }

    #[must_use]
    pub fn name(&self) -> Option<&str> {
        optional_string(&self.object, "name").ok().flatten()
    }

    #[must_use]
    pub const fn hidden(&self) -> bool {
        self.hidden
    }

    #[must_use]
    pub const fn has_effective_guid(&self) -> bool {
        true
    }

    #[must_use]
    pub const fn object(&self) -> &JsonObject {
        &self.object
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct DeserializedProfiles {
    profiles: Vec<DeserializedProfile>,
    warnings: Vec<DeserializationProfileWarning>,
    default_profile_index: usize,
    globals: JsonObject,
}

impl DeserializedProfiles {
    /// Layers a user settings document over an inbox document using Microsoft's
    /// user-first profile ordering semantics.
    ///
    /// User profiles are emitted in user order. If a user profile matches an
    /// inbox identity, the inbox object is first used as its base and the user
    /// layer replaces its members. Unmatched inbox profiles append afterward.
    /// Duplicate user identities keep the first occurrence and emit one warning.
    /// Portable global startup dimensions are clamped to Microsoft's supported
    /// `1..=999` range after layering.
    ///
    /// # Errors
    ///
    /// Returns [`DeserializationProfileError`] for malformed settings shapes,
    /// no effective profiles, or a collection in which every profile is hidden.
    pub fn from_user_and_inbox(
        user_json: &str,
        inbox_json: &str,
    ) -> Result<Self, DeserializationProfileError> {
        let user_root = parse_root(user_json)?;
        let inbox_root = parse_root(inbox_json)?;

        let mut globals = inbox_root.clone();
        for (key, value) in &user_root {
            globals.insert(key.clone(), value.clone());
        }
        clamp_global_i32(&mut globals, "initialCols", 1, 999);
        clamp_global_i32(&mut globals, "initialRows", 1, 999);

        let user_profiles = parse_profiles(&user_root)?;
        let inbox_profiles = parse_profiles(&inbox_root)?;
        let mut inbox_slots = inbox_profiles.into_iter().map(Some).collect::<Vec<_>>();
        let mut profiles = Vec::new();
        let mut seen_user = BTreeSet::new();
        let mut duplicate_profile = false;

        for user_object in user_profiles {
            let user_identity = profile_identity(&user_object)?;
            if !seen_user.insert(user_identity.clone()) {
                duplicate_profile = true;
                continue;
            }

            let matching_index = inbox_slots.iter().position(|candidate| {
                candidate.as_ref().is_some_and(|object| {
                    profile_identity(object).ok().as_ref() == Some(&user_identity)
                })
            });

            let merged = if let Some(index) = matching_index {
                let mut object = inbox_slots[index]
                    .take()
                    .expect("matching inbox profile remains available");
                for (key, value) in user_object {
                    object.insert(key, value);
                }
                object
            } else {
                user_object
            };
            profiles.push(DeserializedProfile::from_object(merged)?);
        }

        for object in inbox_slots.into_iter().flatten() {
            profiles.push(DeserializedProfile::from_object(object)?);
        }

        if profiles.is_empty() {
            return Err(DeserializationProfileError::NoProfiles);
        }
        if profiles.iter().all(DeserializedProfile::hidden) {
            return Err(DeserializationProfileError::AllProfilesHidden);
        }

        let mut warnings = Vec::new();
        if duplicate_profile {
            warnings.push(DeserializationProfileWarning::DuplicateProfile);
        }

        let mut default_profile_index = 0;
        if let Some(requested) = requested_default_profile(&user_root)? {
            if let Some(index) = resolve_default_profile(requested, &profiles) {
                default_profile_index = index;
            } else {
                warnings.push(DeserializationProfileWarning::MissingDefaultProfile);
            }
        }

        Ok(Self {
            profiles,
            warnings,
            default_profile_index,
            globals,
        })
    }

    #[must_use]
    pub fn profiles(&self) -> &[DeserializedProfile] {
        &self.profiles
    }

    #[must_use]
    pub fn profile_count(&self) -> usize {
        self.profiles.len()
    }

    #[must_use]
    pub fn profile_name(&self, index: usize) -> Option<&str> {
        self.profiles.get(index).and_then(DeserializedProfile::name)
    }

    #[must_use]
    pub fn profile_has_effective_guid(&self, index: usize) -> bool {
        self.profiles
            .get(index)
            .is_some_and(DeserializedProfile::has_effective_guid)
    }

    #[must_use]
    pub fn active_profile_count(&self) -> usize {
        self.profiles
            .iter()
            .filter(|profile| !profile.hidden())
            .count()
    }

    #[must_use]
    pub fn active_profile_name(&self, active_index: usize) -> Option<&str> {
        self.profiles
            .iter()
            .filter(|profile| !profile.hidden())
            .nth(active_index)
            .and_then(DeserializedProfile::name)
    }

    #[must_use]
    pub fn warnings(&self) -> &[DeserializationProfileWarning] {
        &self.warnings
    }

    #[must_use]
    pub const fn default_profile_index(&self) -> usize {
        self.default_profile_index
    }

    #[must_use]
    pub fn default_profile_name(&self) -> Option<&str> {
        self.profile_name(self.default_profile_index)
    }

    #[must_use]
    pub fn global_bool(&self, key: &str) -> Option<bool> {
        self.globals.get(key).and_then(JsonValue::as_bool)
    }

    #[must_use]
    pub fn global_i32(&self, key: &str) -> Option<i32> {
        let value = self.globals.get(key).and_then(JsonValue::as_f64)?;
        if value.fract() != 0.0 || value < f64::from(i32::MIN) || value > f64::from(i32::MAX) {
            return None;
        }
        Some(value as i32)
    }
}

fn parse_root(input: &str) -> Result<JsonObject, DeserializationProfileError> {
    let value =
        settings_json::parse(input).map_err(|_| DeserializationProfileError::InvalidJson)?;
    value
        .as_object()
        .cloned()
        .ok_or(DeserializationProfileError::ExpectedRootObject)
}

fn parse_profiles(root: &JsonObject) -> Result<Vec<JsonObject>, DeserializationProfileError> {
    match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => clone_profile_objects(values),
        JsonMember::Value(JsonValue::Object(profiles)) => {
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
                JsonMember::Value(JsonValue::Array(values)) => clone_profile_objects(values),
                JsonMember::Value(_) => Err(DeserializationProfileError::ExpectedProfilesArray),
            }
        }
        JsonMember::Value(_) => Err(DeserializationProfileError::ExpectedProfilesArray),
    }
}

fn clone_profile_objects(
    values: &[JsonValue],
) -> Result<Vec<JsonObject>, DeserializationProfileError> {
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

fn profile_identity(object: &JsonObject) -> Result<ProfileIdentity, DeserializationProfileError> {
    match JsonMember::from_object(object, "guid") {
        JsonMember::Missing | JsonMember::Null => Ok(ProfileIdentity::Generated {
            name: optional_string(object, "name")?
                .unwrap_or_default()
                .to_owned(),
            source: optional_string(object, "source")?.map(ToOwned::to_owned),
        }),
        JsonMember::Value(JsonValue::String(value)) => ProfileGuid::parse(value)
            .map(ProfileIdentity::Guid)
            .map_err(|_| DeserializationProfileError::InvalidGuid),
        JsonMember::Value(_) => Err(DeserializationProfileError::InvalidGuid),
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

fn requested_default_profile(
    root: &JsonObject,
) -> Result<Option<&str>, DeserializationProfileError> {
    let requested = optional_string(root, "defaultProfile")?;
    Ok(requested.filter(|value| !value.is_empty()))
}

fn resolve_default_profile(requested: &str, profiles: &[DeserializedProfile]) -> Option<usize> {
    if let Ok(guid) = ProfileGuid::parse(requested)
        && let Some(index) = profiles
            .iter()
            .position(|profile| profile.identity == ProfileIdentity::Guid(guid))
    {
        return Some(index);
    }
    profiles
        .iter()
        .position(|profile| profile.name() == Some(requested))
}

fn clamp_global_i32(globals: &mut JsonObject, key: &str, min: i32, max: i32) {
    let Some(JsonValue::Number(value)) = globals.get_mut(key) else {
        return;
    };
    if value.fract() != 0.0 || *value < f64::from(i32::MIN) || *value > f64::from(i32::MAX) {
        return;
    }
    *value = f64::from((*value as i32).clamp(min, max));
}
