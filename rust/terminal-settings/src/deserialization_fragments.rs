//! Portable fragment-layer semantics from `SettingsLoader`.
//!
//! Fragments are runtime settings layers, not user-owned serialization. This
//! owner composes the existing profile and action deserialization owners while
//! keeping the fragment-only responsibilities explicit: profile `updates`,
//! ignored fragment keybindings, iterable command expansion and non-persistence
//! across a user-settings roundtrip.

use std::collections::{BTreeMap, BTreeSet};

use crate::{
    deserialization_actions::{DeserializationActionError, DeserializedActionMap},
    deserialization_profiles::{
        DeserializationProfileError, DeserializationProfileWarning, DeserializedProfiles,
    },
    profile::ProfileGuid,
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FragmentError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesArray,
    ExpectedProfileObject,
    ExpectedSchemesArray,
    ExpectedSchemeObject,
    InvalidString,
    InvalidGuid,
    Profiles,
    Actions,
}

impl From<DeserializationProfileError> for FragmentError {
    fn from(_: DeserializationProfileError) -> Self {
        Self::Profiles
    }
}

impl From<DeserializationActionError> for FragmentError {
    fn from(_: DeserializationActionError) -> Self {
        Self::Actions
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct FragmentActionRecord {
    nested_count: usize,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
struct FragmentActionOverlay {
    names: BTreeMap<String, FragmentActionRecord>,
    warnings: usize,
}

impl FragmentActionOverlay {
    fn layer_root(&mut self, root: &JsonObject, scheme_count: usize) -> Result<(), FragmentError> {
        let Some(actions) = root.get("actions") else {
            return Ok(());
        };
        let JsonValue::Array(actions) = actions else {
            return Err(FragmentError::Actions);
        };

        for action in actions {
            let JsonValue::Object(action) = action else {
                return Err(FragmentError::Actions);
            };
            self.layer_action(action, scheme_count)?;
        }
        Ok(())
    }

    fn layer_action(
        &mut self,
        action: &JsonObject,
        scheme_count: usize,
    ) -> Result<(), FragmentError> {
        if let Some(commands) = action.get("commands") {
            return self.layer_nested_action(action, commands, scheme_count);
        }

        // Fragment action `keys` are intentionally ignored. Fragments may
        // contribute commands, but they may not silently install user keybindings.
        let Some(name) = action.get("name").and_then(JsonValue::as_str) else {
            return Ok(());
        };
        if matches!(action.get("command"), None | Some(JsonValue::Null)) {
            self.names.remove(name);
            return Ok(());
        }
        self.names
            .insert(name.to_owned(), FragmentActionRecord { nested_count: 0 });
        Ok(())
    }

    fn layer_nested_action(
        &mut self,
        action: &JsonObject,
        commands: &JsonValue,
        scheme_count: usize,
    ) -> Result<(), FragmentError> {
        // Microsoft intentionally ignores an unnamed nested fragment command
        // without emitting a settings warning.
        let Some(name) = action.get("name").and_then(JsonValue::as_str) else {
            return Ok(());
        };

        if matches!(commands, JsonValue::Null) {
            self.names.remove(name);
            return Ok(());
        }
        let JsonValue::Array(children) = commands else {
            self.warnings += 1;
            return Ok(());
        };

        let mut nested_count = 0_usize;
        for child in children {
            let JsonValue::Object(child) = child else {
                self.warnings += 1;
                return Ok(());
            };
            if child.get("name").and_then(JsonValue::as_str).is_none()
                || (child.get("command").is_none() && child.get("commands").is_none())
            {
                self.warnings += 1;
                return Ok(());
            }
            nested_count += if child.get("iterateOn").and_then(JsonValue::as_str) == Some("schemes")
            {
                scheme_count
            } else {
                1
            };
        }

        self.names
            .insert(name.to_owned(), FragmentActionRecord { nested_count });
        Ok(())
    }
}

/// SettingsLoader-style aggregate with a serializable base and ephemeral
/// fragment runtime layers.
#[derive(Debug, Clone, PartialEq)]
pub struct FragmentSettings {
    profiles: Vec<JsonObject>,
    duplicate_profile: bool,
    scheme_names: BTreeSet<String>,
    base_action_map: DeserializedActionMap,
    fragment_actions: FragmentActionOverlay,
}

impl FragmentSettings {
    /// Builds the normal inbox+user state before fragments are introduced.
    ///
    /// # Errors
    ///
    /// Returns [`FragmentError`] for malformed settings/profile/action/scheme
    /// containers in the portable slice.
    pub fn from_user_and_inbox(user: &str, inbox: &str) -> Result<Self, FragmentError> {
        let layered_profiles = DeserializedProfiles::from_user_and_inbox(user, inbox)?;
        let profiles = layered_profiles
            .profiles()
            .iter()
            .map(|profile| profile.object().clone())
            .collect();
        let duplicate_profile = layered_profiles
            .warnings()
            .contains(&DeserializationProfileWarning::DuplicateProfile);

        let mut scheme_names = BTreeSet::new();
        layer_scheme_names(&mut scheme_names, inbox)?;
        layer_scheme_names(&mut scheme_names, user)?;

        let mut base_action_map = DeserializedActionMap::new();
        base_action_map.layer_settings(inbox)?;
        base_action_map.layer_settings(user)?;

        Ok(Self {
            profiles,
            duplicate_profile,
            scheme_names,
            base_action_map,
            fragment_actions: FragmentActionOverlay::default(),
        })
    }

    /// Applies one fragment as an ephemeral runtime layer.
    ///
    /// Profile `updates` target the already-layered profile identity. New
    /// profiles append. Fragment actions are visible at runtime but never enter
    /// the base action map that represents serialized user-owned settings.
    ///
    /// # Errors
    ///
    /// Returns [`FragmentError`] when the fragment has an invalid portable shape.
    pub fn apply_fragment(&mut self, fragment: &str) -> Result<(), FragmentError> {
        let root = parse_root(fragment)?;
        layer_scheme_names_from_root(&mut self.scheme_names, &root)?;
        self.layer_fragment_profiles(&root)?;
        self.fragment_actions
            .layer_root(&root, self.scheme_names.len())?;
        Ok(())
    }

    fn layer_fragment_profiles(&mut self, root: &JsonObject) -> Result<(), FragmentError> {
        let Some(value) = root.get("profiles") else {
            return Ok(());
        };
        let JsonValue::Array(values) = value else {
            return Err(FragmentError::ExpectedProfilesArray);
        };

        for value in values {
            let JsonValue::Object(profile) = value else {
                return Err(FragmentError::ExpectedProfileObject);
            };
            match JsonMember::from_object(profile, "updates") {
                JsonMember::Missing | JsonMember::Null => self.append_fragment_profile(profile)?,
                JsonMember::Value(JsonValue::String(target)) => {
                    self.update_fragment_profile(target, profile)?;
                }
                JsonMember::Value(_) => return Err(FragmentError::InvalidGuid),
            }
        }
        Ok(())
    }

    fn append_fragment_profile(&mut self, profile: &JsonObject) -> Result<(), FragmentError> {
        if let Some(guid) = object_guid(profile)?
            && self
                .profiles
                .iter()
                .filter_map(|existing| object_guid(existing).ok().flatten())
                .any(|existing| existing == guid)
        {
            self.duplicate_profile = true;
            return Ok(());
        }
        self.profiles.push(profile.clone());
        Ok(())
    }

    fn update_fragment_profile(
        &mut self,
        target: &str,
        update: &JsonObject,
    ) -> Result<(), FragmentError> {
        let target = ProfileGuid::parse(target).map_err(|_| FragmentError::InvalidGuid)?;
        let Some(profile) = self.profiles.iter_mut().find(|profile| {
            object_guid(profile)
                .ok()
                .flatten()
                .is_some_and(|guid| guid == target)
        }) else {
            return Ok(());
        };

        for (key, value) in update {
            if key != "updates" {
                profile.insert(key.clone(), value.clone());
            }
        }
        Ok(())
    }

    #[must_use]
    pub const fn duplicate_profile(&self) -> bool {
        self.duplicate_profile
    }

    #[must_use]
    pub fn profile_count(&self) -> usize {
        self.profiles.len()
    }

    #[must_use]
    pub fn profile_name(&self, index: usize) -> Option<&str> {
        self.profiles
            .get(index)?
            .get("name")
            .and_then(JsonValue::as_str)
    }

    #[must_use]
    pub fn color_scheme_count(&self) -> usize {
        self.scheme_names.len()
    }

    #[must_use]
    pub fn runtime_has_action_named(&self, name: &str) -> bool {
        self.fragment_actions.names.contains_key(name) || self.base_has_action_named(name)
    }

    #[must_use]
    pub fn runtime_action_has_nested_commands(&self, name: &str) -> bool {
        self.fragment_actions
            .names
            .get(name)
            .is_some_and(|record| record.nested_count != 0)
            || self.base_action_map.name_has_nested_commands(name)
    }

    #[must_use]
    pub fn runtime_nested_command_count(&self, name: &str) -> Option<usize> {
        self.fragment_actions
            .names
            .get(name)
            .map(|record| record.nested_count)
            .or_else(|| self.base_action_map.nested_command_count(name))
    }

    #[must_use]
    pub fn runtime_has_keybinding(&self, key: &str) -> bool {
        self.base_action_map.action_id_for_key(key).is_some()
    }

    /// Warning count projected through `CascadiaSettings`' aggregate warning shape.
    #[must_use]
    pub fn warning_count(&self) -> usize {
        let fragment = if self.fragment_actions.warnings == 0 {
            0
        } else {
            self.fragment_actions.warnings + 1
        };
        self.base_action_map.settings_warning_count() + fragment
    }

    /// Observable action state after serializing user settings and reloading
    /// without re-applying fragments. Fragment actions are deliberately absent.
    #[must_use]
    pub fn roundtrip_has_action_named(&self, name: &str) -> bool {
        self.base_has_action_named(name)
    }

    fn base_has_action_named(&self, name: &str) -> bool {
        self.base_action_map.name_action(name).is_some()
            || self.base_action_map.name_has_nested_commands(name)
    }
}

fn parse_root(input: &str) -> Result<JsonObject, FragmentError> {
    let value = settings_json::parse(input).map_err(|_| FragmentError::InvalidJson)?;
    value
        .as_object()
        .cloned()
        .ok_or(FragmentError::ExpectedRootObject)
}

fn layer_scheme_names(names: &mut BTreeSet<String>, input: &str) -> Result<(), FragmentError> {
    let root = parse_root(input)?;
    layer_scheme_names_from_root(names, &root)
}

fn layer_scheme_names_from_root(
    names: &mut BTreeSet<String>,
    root: &JsonObject,
) -> Result<(), FragmentError> {
    let Some(value) = root.get("schemes") else {
        return Ok(());
    };
    let JsonValue::Array(values) = value else {
        return Err(FragmentError::ExpectedSchemesArray);
    };
    for value in values {
        let JsonValue::Object(scheme) = value else {
            return Err(FragmentError::ExpectedSchemeObject);
        };
        let name = scheme
            .get("name")
            .and_then(JsonValue::as_str)
            .ok_or(FragmentError::InvalidString)?;
        names.insert(name.to_owned());
    }
    Ok(())
}

fn object_guid(object: &JsonObject) -> Result<Option<ProfileGuid>, FragmentError> {
    match JsonMember::from_object(object, "guid") {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => ProfileGuid::parse(value)
            .map(Some)
            .map_err(|_| FragmentError::InvalidGuid),
        JsonMember::Value(_) => Err(FragmentError::InvalidGuid),
    }
}
