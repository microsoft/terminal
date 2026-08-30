//! Portable profile identity and prohibited `profiles.defaults` semantics.
//!
//! Microsoft treats `guid`, `name`, `source`, and `commandline` as profile
//! identity/launch fields rather than inheritable defaults. This owner keeps
//! those fields local to each profile, synthesizes the same `UUIDv5` identity as
//! the native product when a GUID is omitted, reconciles legacy inbox/user
//! arrays by that identity, and applies the canonical legacy shell fixups.

use terminal_foundation::{Guid, create_v5_uuid};

use crate::{
    profile::{ProfileGuid, ProfileParseError},
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

const DEFAULT_WINDOWS_POWERSHELL_GUID: &str = "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";
const DEFAULT_COMMAND_PROMPT_GUID: &str = "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}";
const CANONICAL_POWERSHELL_COMMANDLINE: &str =
    "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
const CANONICAL_COMMAND_PROMPT_COMMANDLINE: &str = "%SystemRoot%\\System32\\cmd.exe";
const RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID: Guid = Guid::new(
    0xf65d_db7e,
    0x706b,
    0x4499,
    [0x8a, 0x50, 0x40, 0x31, 0x3c, 0xaf, 0x51, 0x0a],
);

/// Effective identity after Microsoft's lazy GUID generation semantics.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ProfileIdentityGuid {
    Explicit(ProfileGuid),
    Generated([u8; 16]),
}

impl ProfileIdentityGuid {
    #[must_use]
    pub const fn is_zero(self) -> bool {
        match self {
            Self::Explicit(guid) => guid.is_zero(),
            Self::Generated(bytes) => {
                bytes[0] == 0
                    && bytes[1] == 0
                    && bytes[2] == 0
                    && bytes[3] == 0
                    && bytes[4] == 0
                    && bytes[5] == 0
                    && bytes[6] == 0
                    && bytes[7] == 0
                    && bytes[8] == 0
                    && bytes[9] == 0
                    && bytes[10] == 0
                    && bytes[11] == 0
                    && bytes[12] == 0
                    && bytes[13] == 0
                    && bytes[14] == 0
                    && bytes[15] == 0
            }
        }
    }
}

/// Identity/launch fields for one resolved profile.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProfileIdentityRecord {
    name: Option<String>,
    source: Option<String>,
    commandline: Option<String>,
    guid: ProfileIdentityGuid,
}

impl ProfileIdentityRecord {
    fn from_object(object: &JsonObject) -> Result<Self, ProfileParseError> {
        let name = optional_string(object, "name")?;
        let source = optional_string(object, "source")?;
        let commandline = optional_string(object, "commandline")?;
        let guid = identity_guid(object, name.as_deref(), source.as_deref())?;

        let mut record = Self {
            name,
            source,
            commandline,
            guid,
        };
        record.apply_legacy_shell_fixup()?;
        Ok(record)
    }

    fn layer_object(&mut self, object: &JsonObject) -> Result<(), ProfileParseError> {
        if !matches!(JsonMember::from_object(object, "name"), JsonMember::Missing) {
            self.name = optional_string(object, "name")?;
        }
        if !matches!(
            JsonMember::from_object(object, "source"),
            JsonMember::Missing
        ) {
            self.source = optional_string(object, "source")?;
        }
        if !matches!(
            JsonMember::from_object(object, "commandline"),
            JsonMember::Missing
        ) {
            self.commandline = optional_string(object, "commandline")?;
        }
        self.guid = identity_guid(object, self.name.as_deref(), self.source.as_deref())?;
        self.apply_legacy_shell_fixup()
    }

    fn apply_legacy_shell_fixup(&mut self) -> Result<(), ProfileParseError> {
        let powershell = ProfileGuid::parse(DEFAULT_WINDOWS_POWERSHELL_GUID)?;
        let command_prompt = ProfileGuid::parse(DEFAULT_COMMAND_PROMPT_GUID)?;
        match (self.guid, self.commandline.as_deref()) {
            (ProfileIdentityGuid::Explicit(guid), Some(commandline))
                if guid == powershell && commandline.eq_ignore_ascii_case("powershell.exe") =>
            {
                self.commandline = Some(CANONICAL_POWERSHELL_COMMANDLINE.to_owned());
            }
            (ProfileIdentityGuid::Explicit(guid), Some(commandline))
                if guid == command_prompt && commandline.eq_ignore_ascii_case("cmd.exe") =>
            {
                self.commandline = Some(CANONICAL_COMMAND_PROMPT_COMMANDLINE.to_owned());
            }
            _ => {}
        }
        Ok(())
    }

    #[must_use]
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    #[must_use]
    pub fn source(&self) -> Option<&str> {
        self.source.as_deref()
    }

    #[must_use]
    pub fn commandline(&self) -> Option<&str> {
        self.commandline.as_deref()
    }

    #[must_use]
    pub const fn guid(&self) -> ProfileIdentityGuid {
        self.guid
    }
}

/// Resolved identity/launch policy for `profiles.defaults` plus profile lists.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct ProfileIdentitySettings {
    profiles: Vec<ProfileIdentityRecord>,
}

impl ProfileIdentitySettings {
    /// Reconciles legacy top-level profile arrays by explicit or generated GUID.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] for malformed settings or invalid identity
    /// field types.
    pub fn from_layered_legacy_arrays(
        user_json: &str,
        inbox_json: &str,
    ) -> Result<Self, ProfileParseError> {
        let mut profiles = parse_legacy_objects(inbox_json)?
            .iter()
            .map(ProfileIdentityRecord::from_object)
            .collect::<Result<Vec<_>, _>>()?;

        for object in parse_legacy_objects(user_json)? {
            let incoming = ProfileIdentityRecord::from_object(&object)?;
            if let Some(existing) = profiles
                .iter_mut()
                .find(|profile| profile.guid() == incoming.guid())
            {
                existing.layer_object(&object)?;
            } else {
                profiles.push(incoming);
            }
        }

        Ok(Self { profiles })
    }

    /// Parses modern `profiles.defaults`/`profiles.list` while deliberately
    /// excluding `guid`, `name`, `source`, and `commandline` from inheritance.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] for malformed settings or invalid identity
    /// field types.
    pub fn from_modern_json_with_prohibited_defaults(
        input: &str,
    ) -> Result<Self, ProfileParseError> {
        let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
        let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
        let profiles_object = match JsonMember::from_object(root, "profiles") {
            JsonMember::Missing | JsonMember::Null => return Ok(Self::default()),
            JsonMember::Value(JsonValue::Object(value)) => value,
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        };

        if let JsonMember::Value(JsonValue::Object(defaults)) =
            JsonMember::from_object(profiles_object, "defaults")
        {
            for key in ["guid", "name", "source", "commandline"] {
                validate_optional_identity_field(defaults, key)?;
            }
        }

        let values = match JsonMember::from_object(profiles_object, "list") {
            JsonMember::Missing | JsonMember::Null => return Ok(Self::default()),
            JsonMember::Value(JsonValue::Array(values)) => values,
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
        };

        let profiles = values
            .iter()
            .map(|value| {
                value
                    .as_object()
                    .ok_or(ProfileParseError::ExpectedObject)
                    .and_then(ProfileIdentityRecord::from_object)
            })
            .collect::<Result<Vec<_>, _>>()?;
        Ok(Self { profiles })
    }

    #[must_use]
    pub const fn defaults_has_guid(&self) -> bool {
        false
    }

    #[must_use]
    pub const fn defaults_has_name(&self) -> bool {
        false
    }

    #[must_use]
    pub const fn defaults_has_source(&self) -> bool {
        false
    }

    #[must_use]
    pub const fn defaults_has_commandline(&self) -> bool {
        false
    }

    #[must_use]
    pub fn profiles(&self) -> &[ProfileIdentityRecord] {
        &self.profiles
    }
}

fn identity_guid(
    object: &JsonObject,
    name: Option<&str>,
    source: Option<&str>,
) -> Result<ProfileIdentityGuid, ProfileParseError> {
    match JsonMember::from_object(object, "guid") {
        JsonMember::Missing | JsonMember::Null => Ok(ProfileIdentityGuid::Generated(
            generate_profile_guid(name.unwrap_or_default(), source),
        )),
        JsonMember::Value(JsonValue::String(value)) => {
            Ok(ProfileIdentityGuid::Explicit(ProfileGuid::parse(value)?))
        }
        JsonMember::Value(_) => Err(ProfileParseError::InvalidGuid),
    }
}

fn optional_string(object: &JsonObject, key: &str) -> Result<Option<String>, ProfileParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => Ok(Some(value.clone())),
        JsonMember::Value(_) => Err(ProfileParseError::InvalidString),
    }
}

fn validate_optional_identity_field(
    object: &JsonObject,
    key: &str,
) -> Result<(), ProfileParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null | JsonMember::Value(JsonValue::String(_)) => Ok(()),
        JsonMember::Value(_) if key == "guid" => Err(ProfileParseError::InvalidGuid),
        JsonMember::Value(_) => Err(ProfileParseError::InvalidString),
    }
}

fn parse_legacy_objects(input: &str) -> Result<Vec<JsonObject>, ProfileParseError> {
    let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
    let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
    let values = match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => values,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
    };
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

fn utf16le_bytes(value: &str) -> Vec<u8> {
    value.encode_utf16().flat_map(u16::to_le_bytes).collect()
}

fn generate_profile_guid(name: &str, source: Option<&str>) -> [u8; 16] {
    // This is Profile::_GenerateGuidForProfile byte-for-byte: the source, when
    // present, derives a child namespace from the runtime profile namespace;
    // then the UTF-16LE profile name derives the final RFC4122 UUIDv5.
    let namespace = source.filter(|source| !source.is_empty()).map_or(
        RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID,
        |source| {
            create_v5_uuid(
                RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID,
                &utf16le_bytes(source),
            )
        },
    );
    guid_network_bytes(create_v5_uuid(namespace, &utf16le_bytes(name)))
}

fn guid_network_bytes(guid: Guid) -> [u8; 16] {
    let mut bytes = [0_u8; 16];
    bytes[0..4].copy_from_slice(&guid.data1.to_be_bytes());
    bytes[4..6].copy_from_slice(&guid.data2.to_be_bytes());
    bytes[6..8].copy_from_slice(&guid.data3.to_be_bytes());
    bytes[8..16].copy_from_slice(&guid.data4);
    bytes
}
