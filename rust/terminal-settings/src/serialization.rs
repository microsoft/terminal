//! Portable typed settings-document mutation used by serialization contracts.
//!
//! This owner deliberately keeps the parsed JSON tree as the serialization
//! source of truth so unrelated settings survive a targeted mutation. That
//! mirrors `CascadiaSettings::ToJson` for the portable portion of the model
//! without reimplementing `WinRT` projection.

use std::collections::{BTreeMap, BTreeSet};

use crate::{
    color_scheme::ColorSchemeSettings,
    profile::Profile,
    settings_json::{self, JsonObject, JsonValue},
};

const COLOR_SCHEME_TABLE_KEYS: [&str; 16] = [
    "black",
    "red",
    "green",
    "yellow",
    "blue",
    "purple",
    "cyan",
    "white",
    "brightBlack",
    "brightRed",
    "brightGreen",
    "brightYellow",
    "brightBlue",
    "brightPurple",
    "brightCyan",
    "brightWhite",
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SerializationError {
    InvalidJson,
    ExpectedRootObject,
    InvalidProfile,
    InvalidColorSchemeFixup,
    ExpectedSchemesArray,
    ExpectedSchemeObject,
    SchemeNotFound,
    ExpectedProfilesArray,
    ExpectedProfilesDefaultsObject,
    ExpectedProfileObject,
    ExpectedActionsArray,
    ProfileNotFound,
}

#[derive(Debug, Clone, PartialEq)]
pub struct SettingsDocument {
    root: JsonValue,
}

impl SettingsDocument {
    /// Parses one settings JSON/JSONC document while retaining all typed values.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the document is malformed or does
    /// not have the settings root-object shape.
    pub fn from_json(input: &str) -> Result<Self, SerializationError> {
        let root = settings_json::parse(input).map_err(|_| SerializationError::InvalidJson)?;
        if root.as_object().is_none() {
            return Err(SerializationError::ExpectedRootObject);
        }
        Ok(Self { root })
    }

    /// Parses user settings and applies the portable color-scheme collision
    /// write-back performed by `SettingsLoader::FixupUserSettings`.
    ///
    /// The layered [`ColorSchemeSettings`] owner decides which user collisions
    /// are modified and retargets inherited/default references. This serializer
    /// removes user schemes that are semantically identical to inbox schemes,
    /// renames unused modified collisions, writes effective profile-default
    /// references, emits the modern `profiles.list` shape and materializes the
    /// empty actions array expected by Cascadia settings serialization.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] for malformed user/inbox settings,
    /// invalid scheme/profile/action shapes or a color-scheme layering failure.
    pub fn from_json_with_color_scheme_fixup(
        user: &str,
        inbox: &str,
    ) -> Result<Self, SerializationError> {
        let layered = ColorSchemeSettings::from_layers(user, inbox, &[])
            .map_err(|_| SerializationError::InvalidColorSchemeFixup)?;
        let redundant = redundant_user_scheme_names(user, inbox)?;
        let renames = unused_modified_user_scheme_renames(user, inbox, &layered)?;

        let mut document = Self::from_json(user)?;
        document.canonicalize_legacy_profiles()?;
        document.ensure_actions_array()?;
        document.apply_color_scheme_fixup(&layered, &redundant, &renames)?;
        Ok(document)
    }

    /// Parses one profile serialization vector through the safe Rust `Profile`
    /// owner while retaining the complete typed tree for lossless projection.
    ///
    /// The profile owner validates the migrated semantic surface (GUID,
    /// inheritance-backed values, nullable colors/icon, directory and
    /// environment), while the shared JSON tree preserves settings that have
    /// not yet moved into that owner. Legacy top-level font aliases are
    /// canonicalized into the modern `font` object, matching `Profile::ToJson`.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError::InvalidProfile`] when the migrated profile
    /// semantics reject the vector, or another serialization error when the
    /// JSON itself cannot be retained as a root object.
    pub fn from_profile_json(input: &str) -> Result<Self, SerializationError> {
        Profile::from_json(input).map_err(|_| SerializationError::InvalidProfile)?;
        let mut document = Self::from_json(input)?;
        document.canonicalize_legacy_profile_font()?;
        Ok(document)
    }

    /// Canonicalizes the legacy root `profiles: []` shape to the modern
    /// `profiles: { "list": [] }` shape used by `CascadiaSettings` serialization.
    /// Existing modern profile objects are preserved unchanged. When legacy
    /// `compatibility.reloadEnvironmentVariables` is present at the root, it is
    /// moved into `profiles.defaults`, matching the `SettingsLoader` fixup.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the settings root is not an object,
    /// when `profiles` is present with a shape other than an array/object, or
    /// when an existing `profiles.defaults` value is not an object.
    pub fn canonicalize_legacy_profiles(&mut self) -> Result<(), SerializationError> {
        let root = self.root_object_mut()?;
        if let Some(profiles) = root.remove("profiles") {
            match profiles {
                JsonValue::Array(list) => {
                    let mut modern = JsonObject::new();
                    modern.insert("list".to_owned(), JsonValue::Array(list));
                    root.insert("profiles".to_owned(), JsonValue::Object(modern));
                }
                JsonValue::Object(object) => {
                    root.insert("profiles".to_owned(), JsonValue::Object(object));
                }
                other => {
                    root.insert("profiles".to_owned(), other);
                    return Err(SerializationError::ExpectedProfilesArray);
                }
            }
        }

        let Some(reload_environment_variables) =
            root.remove("compatibility.reloadEnvironmentVariables")
        else {
            return Ok(());
        };

        let Some(JsonValue::Object(profiles)) = root.get_mut("profiles") else {
            root.insert(
                "compatibility.reloadEnvironmentVariables".to_owned(),
                reload_environment_variables,
            );
            return Ok(());
        };

        let defaults = profiles
            .entry("defaults".to_owned())
            .or_insert_with(|| JsonValue::Object(JsonObject::new()));
        let JsonValue::Object(defaults) = defaults else {
            return Err(SerializationError::ExpectedProfilesDefaultsObject);
        };
        defaults
            .entry("compatibility.reloadEnvironmentVariables".to_owned())
            .or_insert(reload_environment_variables);
        Ok(())
    }

    /// Changes the foreground of one named user color scheme in-place.
    /// Unrelated members remain exactly represented by the shared typed tree.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] for an invalid schemes shape or if the
    /// requested scheme is absent.
    pub fn set_color_scheme_foreground(
        &mut self,
        name: &str,
        foreground: &str,
    ) -> Result<(), SerializationError> {
        let root = self.root_object_mut()?;
        let schemes = root
            .get_mut("schemes")
            .ok_or(SerializationError::ExpectedSchemesArray)?;
        let schemes = match schemes {
            JsonValue::Array(schemes) => schemes,
            _ => return Err(SerializationError::ExpectedSchemesArray),
        };

        for scheme in schemes {
            let scheme = match scheme {
                JsonValue::Object(scheme) => scheme,
                _ => return Err(SerializationError::ExpectedSchemeObject),
            };
            if scheme.get("name").and_then(JsonValue::as_str) == Some(name) {
                scheme.insert(
                    "foreground".to_owned(),
                    JsonValue::String(foreground.to_owned()),
                );
                return Ok(());
            }
        }

        Err(SerializationError::SchemeNotFound)
    }

    /// Sets an integer member on the settings root while preserving all
    /// unrelated serialized members.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the settings root is not an object.
    pub fn set_global_i32(&mut self, member: &str, value: i32) -> Result<(), SerializationError> {
        self.root_object_mut()?
            .insert(member.to_owned(), JsonValue::Number(f64::from(value)));
        Ok(())
    }

    /// Sets a boolean member on the settings root while preserving all
    /// unrelated serialized members.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the settings root is not an object.
    pub fn set_global_bool(&mut self, member: &str, value: bool) -> Result<(), SerializationError> {
        self.root_object_mut()?
            .insert(member.to_owned(), JsonValue::Bool(value));
        Ok(())
    }

    /// Sets an integer member on the indexed profile while preserving all
    /// unrelated serialized members.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the profiles shape is invalid or the
    /// requested profile index is absent.
    pub fn set_profile_i32(
        &mut self,
        index: usize,
        member: &str,
        value: i32,
    ) -> Result<(), SerializationError> {
        self.profile_object_mut(index)?
            .insert(member.to_owned(), JsonValue::Number(f64::from(value)));
        Ok(())
    }

    /// Sets a string member on the indexed profile while preserving all
    /// unrelated serialized members.
    ///
    /// # Errors
    ///
    /// Returns [`SerializationError`] when the profiles shape is invalid or the
    /// requested profile index is absent.
    pub fn set_profile_string(
        &mut self,
        index: usize,
        member: &str,
        value: &str,
    ) -> Result<(), SerializationError> {
        self.profile_object_mut(index)?
            .insert(member.to_owned(), JsonValue::String(value.to_owned()));
        Ok(())
    }

    #[must_use]
    pub const fn to_json_value(&self) -> &JsonValue {
        &self.root
    }

    fn root_object_mut(&mut self) -> Result<&mut JsonObject, SerializationError> {
        match &mut self.root {
            JsonValue::Object(root) => Ok(root),
            _ => Err(SerializationError::ExpectedRootObject),
        }
    }

    fn ensure_actions_array(&mut self) -> Result<(), SerializationError> {
        let root = self.root_object_mut()?;
        if !root.contains_key("actions") {
            root.insert("actions".to_owned(), JsonValue::Array(Vec::new()));
            return Ok(());
        }
        if matches!(root.get("actions"), Some(JsonValue::Array(_))) {
            Ok(())
        } else {
            Err(SerializationError::ExpectedActionsArray)
        }
    }

    fn apply_color_scheme_fixup(
        &mut self,
        layered: &ColorSchemeSettings,
        redundant: &BTreeSet<String>,
        renames: &BTreeMap<String, String>,
    ) -> Result<(), SerializationError> {
        let defaults = layered.profile_defaults();
        let light = defaults
            .has_light_name()
            .then(|| defaults.light_name().to_owned());
        let dark = defaults
            .has_dark_name()
            .then(|| defaults.dark_name().to_owned());

        let root = self.root_object_mut()?;
        if let Some(value) = root.get_mut("schemes") {
            let JsonValue::Array(schemes) = value else {
                return Err(SerializationError::ExpectedSchemesArray);
            };
            for value in schemes.iter_mut() {
                let JsonValue::Object(scheme) = value else {
                    return Err(SerializationError::ExpectedSchemeObject);
                };
                let name = scheme
                    .get("name")
                    .and_then(JsonValue::as_str)
                    .ok_or(SerializationError::ExpectedSchemeObject)?
                    .to_owned();
                if let Some(target) = renames.get(&name) {
                    scheme.insert("name".to_owned(), JsonValue::String(target.clone()));
                }
            }
            schemes.retain(|value| {
                let remove = value
                    .as_object()
                    .and_then(|scheme| scheme.get("name"))
                    .and_then(JsonValue::as_str)
                    .is_some_and(|name| redundant.contains(name));
                !remove
            });
        }

        if light.is_none() && dark.is_none() {
            return Ok(());
        }

        let profiles = root
            .get_mut("profiles")
            .ok_or(SerializationError::ExpectedProfilesArray)?;
        let JsonValue::Object(profiles) = profiles else {
            return Err(SerializationError::ExpectedProfilesArray);
        };
        let defaults = profiles
            .entry("defaults".to_owned())
            .or_insert_with(|| JsonValue::Object(JsonObject::new()));
        let JsonValue::Object(defaults) = defaults else {
            return Err(SerializationError::ExpectedProfilesDefaultsObject);
        };

        let color_scheme = match (light, dark) {
            (Some(light), Some(dark)) if light == dark => JsonValue::String(light),
            (light, dark) => {
                let mut value = JsonObject::new();
                if let Some(light) = light {
                    value.insert("light".to_owned(), JsonValue::String(light));
                }
                if let Some(dark) = dark {
                    value.insert("dark".to_owned(), JsonValue::String(dark));
                }
                JsonValue::Object(value)
            }
        };
        defaults.insert("colorScheme".to_owned(), color_scheme);
        Ok(())
    }

    fn canonicalize_legacy_profile_font(&mut self) -> Result<(), SerializationError> {
        let root = self.root_object_mut()?;
        let face = root.remove("fontFace");
        let size = root.remove("fontSize");
        let weight = root.remove("fontWeight");

        if face.is_none() && size.is_none() && weight.is_none() {
            return Ok(());
        }

        let font = root
            .entry("font".to_owned())
            .or_insert_with(|| JsonValue::Object(JsonObject::new()));
        let JsonValue::Object(font) = font else {
            return Err(SerializationError::InvalidProfile);
        };

        if let Some(value) = face {
            font.entry("face".to_owned()).or_insert(value);
        }
        if let Some(value) = size {
            font.entry("size".to_owned()).or_insert(value);
        }
        if let Some(value) = weight {
            font.entry("weight".to_owned()).or_insert(value);
        }
        Ok(())
    }

    fn profile_object_mut(&mut self, index: usize) -> Result<&mut JsonObject, SerializationError> {
        let root = self.root_object_mut()?;
        let profiles = root
            .get_mut("profiles")
            .ok_or(SerializationError::ExpectedProfilesArray)?;
        let profiles = match profiles {
            JsonValue::Array(profiles) => profiles,
            JsonValue::Object(profiles) => match profiles.get_mut("list") {
                Some(JsonValue::Array(profiles)) => profiles,
                _ => return Err(SerializationError::ExpectedProfilesArray),
            },
            _ => return Err(SerializationError::ExpectedProfilesArray),
        };
        let profile = profiles
            .get_mut(index)
            .ok_or(SerializationError::ProfileNotFound)?;
        match profile {
            JsonValue::Object(profile) => Ok(profile),
            _ => Err(SerializationError::ExpectedProfileObject),
        }
    }
}

fn redundant_user_scheme_names(
    user: &str,
    inbox: &str,
) -> Result<BTreeSet<String>, SerializationError> {
    let user = settings_json::parse(user).map_err(|_| SerializationError::InvalidJson)?;
    let inbox = settings_json::parse(inbox).map_err(|_| SerializationError::InvalidJson)?;
    let user = user
        .as_object()
        .ok_or(SerializationError::ExpectedRootObject)?;
    let inbox = inbox
        .as_object()
        .ok_or(SerializationError::ExpectedRootObject)?;
    let user_schemes = schemes_array(user)?;
    let inbox_schemes = schemes_array(inbox)?;

    let mut redundant = BTreeSet::new();
    for user_scheme in user_schemes {
        let user_scheme = user_scheme
            .as_object()
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        let name = user_scheme
            .get("name")
            .and_then(JsonValue::as_str)
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        for inbox_scheme in inbox_schemes {
            let inbox_scheme = inbox_scheme
                .as_object()
                .ok_or(SerializationError::ExpectedSchemeObject)?;
            if inbox_scheme.get("name").and_then(JsonValue::as_str) == Some(name)
                && equivalent_for_settings_merge(user_scheme, inbox_scheme)?
            {
                redundant.insert(name.to_owned());
                break;
            }
        }
    }
    Ok(redundant)
}

fn unused_modified_user_scheme_renames(
    user: &str,
    inbox: &str,
    layered: &ColorSchemeSettings,
) -> Result<BTreeMap<String, String>, SerializationError> {
    let user = settings_json::parse(user).map_err(|_| SerializationError::InvalidJson)?;
    let inbox = settings_json::parse(inbox).map_err(|_| SerializationError::InvalidJson)?;
    let user = user
        .as_object()
        .ok_or(SerializationError::ExpectedRootObject)?;
    let inbox = inbox
        .as_object()
        .ok_or(SerializationError::ExpectedRootObject)?;
    let user_schemes = schemes_array(user)?;
    let inbox_schemes = schemes_array(inbox)?;

    let mut reserved_user_names = BTreeSet::new();
    for value in user_schemes {
        let scheme = value
            .as_object()
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        let name = scheme
            .get("name")
            .and_then(JsonValue::as_str)
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        reserved_user_names.insert(name.to_owned());
    }

    let mut occupied_names = BTreeSet::new();
    for value in inbox_schemes {
        let scheme = value
            .as_object()
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        let name = scheme
            .get("name")
            .and_then(JsonValue::as_str)
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        occupied_names.insert(name.to_owned());
    }

    let mut renames = BTreeMap::new();
    for value in user_schemes {
        let user_scheme = value
            .as_object()
            .ok_or(SerializationError::ExpectedSchemeObject)?;
        let name = user_scheme
            .get("name")
            .and_then(JsonValue::as_str)
            .ok_or(SerializationError::ExpectedSchemeObject)?;

        let inbox_scheme = inbox_schemes.iter().find_map(|value| {
            let scheme = value.as_object()?;
            (scheme.get("name").and_then(JsonValue::as_str) == Some(name)).then_some(scheme)
        });

        let Some(inbox_scheme) = inbox_scheme else {
            occupied_names.insert(name.to_owned());
            continue;
        };
        if equivalent_for_settings_merge(user_scheme, inbox_scheme)? {
            continue;
        }

        let target = next_modified_name(name, &occupied_names, &reserved_user_names);
        if !color_scheme_name_is_effectively_referenced(layered, &target) {
            renames.insert(name.to_owned(), target.clone());
        }
        occupied_names.insert(target);
    }
    Ok(renames)
}

fn next_modified_name(
    original: &str,
    occupied_names: &BTreeSet<String>,
    reserved_user_names: &BTreeSet<String>,
) -> String {
    let first = format!("{original} (modified)");
    if !occupied_names.contains(&first) && !reserved_user_names.contains(&first) {
        return first;
    }
    for index in 2_u32.. {
        let candidate = format!("{original} (modified {index})");
        if !occupied_names.contains(&candidate) && !reserved_user_names.contains(&candidate) {
            return candidate;
        }
    }
    unreachable!("u32 candidate space cannot be exhausted by a settings document")
}

fn color_scheme_name_is_effectively_referenced(layered: &ColorSchemeSettings, name: &str) -> bool {
    let defaults = layered.profile_defaults();
    if defaults.light_name() == name || defaults.dark_name() == name {
        return true;
    }

    layered.profiles().iter().any(|profile| {
        let default = profile.default_appearance();
        let unfocused = profile.unfocused_appearance();
        default.light_name() == name
            || default.dark_name() == name
            || unfocused.light_name() == name
            || unfocused.dark_name() == name
    })
}

fn schemes_array(root: &JsonObject) -> Result<&[JsonValue], SerializationError> {
    match root.get("schemes") {
        None => Ok(&[]),
        Some(JsonValue::Array(schemes)) => Ok(schemes),
        Some(_) => Err(SerializationError::ExpectedSchemesArray),
    }
}

fn equivalent_for_settings_merge(
    left: &JsonObject,
    right: &JsonObject,
) -> Result<bool, SerializationError> {
    if color_member(left, "background", "#000000")? != color_member(right, "background", "#000000")?
        || color_member(left, "foreground", "#C0C0C0")?
            != color_member(right, "foreground", "#C0C0C0")?
    {
        return Ok(false);
    }

    for key in COLOR_SCHEME_TABLE_KEYS {
        if required_color_member(left, key)? != required_color_member(right, key)? {
            return Ok(false);
        }
    }
    Ok(true)
}

fn color_member(object: &JsonObject, key: &str, default: &str) -> Result<u32, SerializationError> {
    match object.get(key) {
        None => parse_rgb(default).ok_or(SerializationError::InvalidColorSchemeFixup),
        Some(JsonValue::String(value)) => {
            parse_rgb(value).ok_or(SerializationError::InvalidColorSchemeFixup)
        }
        Some(_) => Err(SerializationError::InvalidColorSchemeFixup),
    }
}

fn required_color_member(object: &JsonObject, key: &str) -> Result<u32, SerializationError> {
    match object.get(key) {
        Some(JsonValue::String(value)) => {
            parse_rgb(value).ok_or(SerializationError::InvalidColorSchemeFixup)
        }
        _ => Err(SerializationError::InvalidColorSchemeFixup),
    }
}

fn parse_rgb(value: &str) -> Option<u32> {
    if value.len() != 7 || !value.starts_with('#') || !value.is_ascii() {
        return None;
    }
    u32::from_str_radix(&value[1..], 16).ok()
}
