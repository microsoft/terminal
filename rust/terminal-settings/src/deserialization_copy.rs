//! Deep-copy and inheritance-graph semantics for portable `CascadiaSettings`.
//!
//! Microsoft's `CascadiaSettings::Copy` must clone owned settings collections
//! while reconnecting inherited profile values to the copied `profiles.defaults`
//! node. This owner keeps that graph behavior portable without introducing
//! WinRT/reference-counting concerns into the simpler profile inheritance owner.

use std::collections::BTreeSet;

use crate::{
    deserialization_actions::{DeserializationActionError, DeserializedActionMap},
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

const DEFAULT_SNAP_ON_INPUT: bool = true;
const DEFAULT_WORD_DELIMITERS: &str = "";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeserializationCopyError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesObject,
    ExpectedProfilesArray,
    ExpectedProfileObject,
    ExpectedDefaultsObject,
    ExpectedSchemesArray,
    ExpectedSchemeObject,
    InvalidString,
    InvalidBoolean,
    Actions(DeserializationActionError),
}

/// Profile locals plus a live `profiles.defaults` node.
///
/// Profiles intentionally retain their local JSON instead of materializing
/// inherited values. Resolved accessors therefore follow the current defaults
/// node, which is what makes mutation of a cloned inheritance tree observable
/// only inside that clone.
#[derive(Debug, Clone, PartialEq, Default)]
struct CloneableProfileGraph {
    defaults: JsonObject,
    defaults_contributes_parent: bool,
    profiles: Vec<JsonObject>,
}

impl CloneableProfileGraph {
    fn from_root(root: &JsonObject) -> Result<Self, DeserializationCopyError> {
        match JsonMember::from_object(root, "profiles") {
            JsonMember::Missing | JsonMember::Null => Ok(Self::default()),
            JsonMember::Value(JsonValue::Array(values)) => Ok(Self {
                defaults: JsonObject::new(),
                defaults_contributes_parent: false,
                profiles: parse_profile_objects(values)?,
            }),
            JsonMember::Value(JsonValue::Object(profiles)) => {
                let defaults = match JsonMember::from_object(profiles, "defaults") {
                    JsonMember::Missing | JsonMember::Null => JsonObject::new(),
                    JsonMember::Value(JsonValue::Object(defaults)) => defaults.clone(),
                    JsonMember::Value(_) => {
                        return Err(DeserializationCopyError::ExpectedDefaultsObject);
                    }
                };
                validate_profile_layer(&defaults)?;
                let defaults_contributes_parent = !defaults.is_empty();
                let profile_values = match JsonMember::from_object(profiles, "list") {
                    JsonMember::Missing | JsonMember::Null => Vec::new(),
                    JsonMember::Value(JsonValue::Array(values)) => parse_profile_objects(values)?,
                    JsonMember::Value(_) => {
                        return Err(DeserializationCopyError::ExpectedProfilesArray);
                    }
                };
                Ok(Self {
                    defaults,
                    defaults_contributes_parent,
                    profiles: profile_values,
                })
            }
            JsonMember::Value(_) => Err(DeserializationCopyError::ExpectedProfilesObject),
        }
    }

    fn resolved_string(&self, index: usize, key: &str) -> Option<&str> {
        let profile = self.profiles.get(index)?;
        profile
            .get(key)
            .and_then(JsonValue::as_str)
            .or_else(|| self.defaults.get(key).and_then(JsonValue::as_str))
    }

    fn resolved_bool(&self, index: usize, key: &str, built_in: bool) -> Option<bool> {
        let profile = self.profiles.get(index)?;
        Some(
            profile
                .get(key)
                .and_then(JsonValue::as_bool)
                .or_else(|| self.defaults.get(key).and_then(JsonValue::as_bool))
                .unwrap_or(built_in),
        )
    }

    fn has_local(&self, index: usize, key: &str) -> bool {
        self.profiles
            .get(index)
            .is_some_and(|profile| profile.contains_key(key))
    }
}

/// Portable aggregate for Microsoft's `CascadiaSettings::Copy` observables.
#[derive(Debug, Clone, PartialEq)]
pub struct CloneableCascadiaSettings {
    globals: JsonObject,
    profiles: CloneableProfileGraph,
    color_scheme_names: BTreeSet<String>,
    action_map: DeserializedActionMap,
}

impl CloneableCascadiaSettings {
    /// Parses the portable collections and profile graph needed by the native
    /// `TestCopy` / `TestCloneInheritanceTree` contracts.
    ///
    /// # Errors
    ///
    /// Returns [`DeserializationCopyError`] for malformed settings containers or
    /// migrated values with the wrong shape.
    pub fn from_json(input: &str) -> Result<Self, DeserializationCopyError> {
        let value =
            settings_json::parse(input).map_err(|_| DeserializationCopyError::InvalidJson)?;
        let root = value
            .as_object()
            .cloned()
            .ok_or(DeserializationCopyError::ExpectedRootObject)?;

        validate_optional_string(&root, "defaultProfile")?;
        validate_optional_string(&root, "wordDelimiters")?;
        let profiles = CloneableProfileGraph::from_root(&root)?;
        let color_scheme_names = parse_color_scheme_names(&root)?;
        let mut action_map = DeserializedActionMap::new();
        action_map
            .layer_settings(input)
            .map_err(DeserializationCopyError::Actions)?;

        Ok(Self {
            globals: root,
            profiles,
            color_scheme_names,
            action_map,
        })
    }

    /// Deep-copy the aggregate. All containers are owned, while inherited
    /// profile lookups remain connected to the copied defaults node.
    #[must_use]
    pub fn deep_copy(&self) -> Self {
        self.clone()
    }

    #[must_use]
    pub fn default_profile(&self) -> Option<&str> {
        self.globals
            .get("defaultProfile")
            .and_then(JsonValue::as_str)
    }

    #[must_use]
    pub fn word_delimiters(&self) -> &str {
        self.globals
            .get("wordDelimiters")
            .and_then(JsonValue::as_str)
            .unwrap_or(DEFAULT_WORD_DELIMITERS)
    }

    pub fn set_word_delimiters(&mut self, value: &str) {
        self.globals.insert(
            "wordDelimiters".to_owned(),
            JsonValue::String(value.to_owned()),
        );
    }

    #[must_use]
    pub fn profile_count(&self) -> usize {
        self.profiles.profiles.len()
    }

    #[must_use]
    pub fn active_profile_count(&self) -> usize {
        self.profiles
            .profiles
            .iter()
            .filter(|profile| profile.get("hidden").and_then(JsonValue::as_bool) != Some(true))
            .count()
    }

    #[must_use]
    pub fn profile_name(&self, index: usize) -> Option<&str> {
        self.profiles.resolved_string(index, "name")
    }

    #[must_use]
    pub fn profile_tab_title(&self, index: usize) -> Option<&str> {
        self.profiles.resolved_string(index, "tabTitle")
    }

    #[must_use]
    pub fn profile_snap_on_input(&self, index: usize) -> Option<bool> {
        self.profiles
            .resolved_bool(index, "snapOnInput", DEFAULT_SNAP_ON_INPUT)
    }

    #[must_use]
    pub fn profile_has_snap_on_input(&self, index: usize) -> bool {
        self.profiles.has_local(index, "snapOnInput")
    }

    /// Sets a local profile value, matching `HasSnapOnInput` becoming true on
    /// that profile only.
    pub fn set_profile_snap_on_input(&mut self, index: usize, value: bool) -> bool {
        let Some(profile) = self.profiles.profiles.get_mut(index) else {
            return false;
        };
        profile.insert("snapOnInput".to_owned(), JsonValue::Bool(value));
        true
    }

    /// `ProfileDefaults()` remains a real node even when the user omitted or
    /// supplied an empty `profiles.defaults` object.
    #[must_use]
    pub const fn has_profile_defaults(&self) -> bool {
        true
    }

    #[must_use]
    pub fn profile_defaults_tab_title(&self) -> Option<&str> {
        self.profiles
            .defaults
            .get("tabTitle")
            .and_then(JsonValue::as_str)
    }

    #[must_use]
    pub fn profile_defaults_has_tab_title(&self) -> bool {
        self.profiles.defaults.contains_key("tabTitle")
    }

    pub fn set_profile_defaults_tab_title(&mut self, value: &str) {
        self.profiles
            .defaults
            .insert("tabTitle".to_owned(), JsonValue::String(value.to_owned()));
        self.profiles.defaults_contributes_parent = true;
    }

    /// Number of inheritance parents visible from a profile in this portable
    /// slice. A non-empty profile-defaults node contributes the extra parent;
    /// empty/missing defaults are retained as a node but optimized out of the
    /// parent chain, matching the native contract.
    #[must_use]
    pub fn profile_parent_count(&self, index: usize) -> Option<usize> {
        self.profiles.profiles.get(index)?;
        Some(if self.profiles.defaults_contributes_parent {
            2
        } else {
            1
        })
    }

    #[must_use]
    pub fn color_scheme_count(&self) -> usize {
        self.color_scheme_names.len()
    }

    #[must_use]
    pub fn has_color_scheme(&self, name: &str) -> bool {
        self.color_scheme_names.contains(name)
    }

    #[must_use]
    pub fn keybinding_count(&self) -> usize {
        self.action_map.keybinding_count()
    }

    #[must_use]
    pub fn action_name_count(&self) -> usize {
        self.action_map.name_count()
    }
}

fn parse_profile_objects(
    values: &[JsonValue],
) -> Result<Vec<JsonObject>, DeserializationCopyError> {
    values
        .iter()
        .map(|value| {
            let profile = value
                .as_object()
                .cloned()
                .ok_or(DeserializationCopyError::ExpectedProfileObject)?;
            validate_profile_layer(&profile)?;
            Ok(profile)
        })
        .collect()
}

fn validate_profile_layer(profile: &JsonObject) -> Result<(), DeserializationCopyError> {
    validate_optional_string(profile, "name")?;
    validate_optional_string(profile, "tabTitle")?;
    validate_optional_bool(profile, "snapOnInput")?;
    validate_optional_bool(profile, "hidden")?;
    Ok(())
}

fn validate_optional_string(
    object: &JsonObject,
    key: &str,
) -> Result<(), DeserializationCopyError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null | JsonMember::Value(JsonValue::String(_)) => Ok(()),
        JsonMember::Value(_) => Err(DeserializationCopyError::InvalidString),
    }
}

fn validate_optional_bool(object: &JsonObject, key: &str) -> Result<(), DeserializationCopyError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null | JsonMember::Value(JsonValue::Bool(_)) => Ok(()),
        JsonMember::Value(_) => Err(DeserializationCopyError::InvalidBoolean),
    }
}

fn parse_color_scheme_names(
    root: &JsonObject,
) -> Result<BTreeSet<String>, DeserializationCopyError> {
    let Some(value) = root.get("schemes") else {
        return Ok(BTreeSet::new());
    };
    let JsonValue::Array(values) = value else {
        return Err(DeserializationCopyError::ExpectedSchemesArray);
    };
    values
        .iter()
        .map(|value| {
            let scheme = value
                .as_object()
                .ok_or(DeserializationCopyError::ExpectedSchemeObject)?;
            scheme
                .get("name")
                .and_then(JsonValue::as_str)
                .map(str::to_owned)
                .ok_or(DeserializationCopyError::InvalidString)
        })
        .collect()
}
