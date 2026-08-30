//! Portable `CascadiaSettings` profile lookup projection.
//!
//! Profile identity already reproduces Microsoft's explicit and generated `UUIDv5`
//! semantics. This thin product projection adds the name/GUID lookup operations
//! exposed by `CascadiaSettings` without duplicating identity generation.

use crate::profile::{ProfileGuid, ProfileParseError};
use crate::profile_identity::{
    ProfileIdentityGuid, ProfileIdentityRecord, ProfileIdentitySettings,
};

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct ProfileLookup {
    settings: ProfileIdentitySettings,
}

impl ProfileLookup {
    /// Builds lookup state from the legacy top-level profile list used by the
    /// deserialization helper contract.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when a profile identity is malformed.
    pub fn from_legacy_user_json(user_json: &str) -> Result<Self, ProfileParseError> {
        let settings =
            ProfileIdentitySettings::from_layered_legacy_arrays(user_json, r#"{"profiles": []}"#)?;
        Ok(Self { settings })
    }

    #[must_use]
    pub fn profiles(&self) -> &[ProfileIdentityRecord] {
        self.settings.profiles()
    }

    #[must_use]
    pub fn get_profile_by_name(&self, name: &str) -> Option<&ProfileIdentityRecord> {
        self.settings
            .profiles()
            .iter()
            .find(|profile| profile.name() == Some(name))
    }

    #[must_use]
    pub fn find_profile(&self, guid: ProfileIdentityGuid) -> Option<&ProfileIdentityRecord> {
        self.settings
            .profiles()
            .iter()
            .find(|profile| profile.guid() == guid)
    }

    #[must_use]
    pub fn find_explicit_profile(&self, guid: ProfileGuid) -> Option<&ProfileIdentityRecord> {
        self.find_profile(ProfileIdentityGuid::Explicit(guid))
    }
}
