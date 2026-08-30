//! Portable root settings-document owner for `CascadiaSettings` serialization.
//!
//! This type composes the shared [`SettingsDocument`] owner instead of creating
//! a second serializer. It validates the portable aggregate shapes that belong
//! to the settings root while retaining the complete typed JSON tree for
//! lossless round-trip projection.

use crate::{
    serialization::{SerializationError, SettingsDocument},
    settings_json::JsonValue,
};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CascadiaSettingsError {
    Serialization(SerializationError),
    InvalidProfilesShape,
    InvalidSchemesShape,
    InvalidActionsShape,
    InvalidKeybindingsShape,
}

#[derive(Debug, Clone, PartialEq)]
pub struct CascadiaSettingsDocument {
    document: SettingsDocument,
    fixups_applied_during_load: bool,
}

impl CascadiaSettingsDocument {
    /// Parses a complete settings document through the shared serialization
    /// owner and validates portable aggregate members when they are present.
    /// Legacy root profile arrays are canonicalized to `profiles.list`, matching
    /// the modern shape emitted by `CascadiaSettings` serialization. The portable
    /// load-fixup flag records migration of legacy root reload-environment state.
    ///
    /// # Errors
    ///
    /// Returns an error for malformed JSON/root shape or for aggregate members
    /// whose JSON shape cannot represent Cascadia settings.
    pub fn from_json(input: &str) -> Result<Self, CascadiaSettingsError> {
        let mut document =
            SettingsDocument::from_json(input).map_err(CascadiaSettingsError::Serialization)?;
        let fixups_applied_during_load = document
            .to_json_value()
            .as_object()
            .is_some_and(|root| root.contains_key("compatibility.reloadEnvironmentVariables"));
        document
            .canonicalize_legacy_profiles()
            .map_err(CascadiaSettingsError::Serialization)?;
        let root = document
            .to_json_value()
            .as_object()
            .expect("SettingsDocument guarantees an object root");

        if let Some(profiles) = root.get("profiles") {
            match profiles {
                JsonValue::Object(object)
                    if matches!(object.get("list"), Some(JsonValue::Array(_))) => {}
                _ => return Err(CascadiaSettingsError::InvalidProfilesShape),
            }
        }
        if !matches!(root.get("schemes"), None | Some(JsonValue::Array(_))) {
            return Err(CascadiaSettingsError::InvalidSchemesShape);
        }
        if !matches!(root.get("actions"), None | Some(JsonValue::Array(_))) {
            return Err(CascadiaSettingsError::InvalidActionsShape);
        }
        if !matches!(root.get("keybindings"), None | Some(JsonValue::Array(_))) {
            return Err(CascadiaSettingsError::InvalidKeybindingsShape);
        }

        Ok(Self {
            document,
            fixups_applied_during_load,
        })
    }

    #[must_use]
    pub const fn to_json_value(&self) -> &JsonValue {
        self.document.to_json_value()
    }

    /// Reports whether a portable legacy setting required a load-time migration.
    #[must_use]
    pub const fn fixups_applied_during_load(&self) -> bool {
        self.fixups_applied_during_load
    }

    /// Reads one boolean value from modern `profiles.defaults` after load fixups.
    #[must_use]
    pub fn profile_default_bool(&self, key: &str) -> Option<bool> {
        self.document
            .to_json_value()
            .as_object()?
            .get("profiles")?
            .as_object()?
            .get("defaults")?
            .as_object()?
            .get(key)?
            .as_bool()
    }
}
