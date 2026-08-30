//! Deterministic profile duplication semantics from `CascadiaSettings::DuplicateProfile`.
//!
//! The native implementation creates a fresh random GUID and localized copy name,
//! but the Microsoft contract deliberately overwrites those two values before
//! comparing serialization. This owner therefore keeps that nondeterministic/UI
//! boundary out of Rust while owning the observable settings behavior: local
//! profile settings are duplicated, `profiles.defaults` remains inherited rather
//! than materialized into JSON, and nested font defaults continue to resolve.

use crate::{
    profile::ProfileParseError,
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

/// One profile layer together with the `profiles.defaults` layer it inherits.
#[derive(Debug, Clone, PartialEq)]
pub struct DuplicableProfile {
    defaults: JsonObject,
    local: JsonObject,
}

impl DuplicableProfile {
    /// Returns the resolved font size without copying the defaults-owned value
    /// into this profile's serialized local layer.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] if `font`/`font.size` has the wrong shape.
    pub fn font_size(&self) -> Result<Option<i32>, ProfileParseError> {
        if let Some(value) = font_size_from_object(&self.local)? {
            return Ok(Some(value));
        }
        font_size_from_object(&self.defaults)
    }

    #[must_use]
    pub fn name(&self) -> Option<&str> {
        optional_string(&self.local, "name")
    }

    #[must_use]
    pub fn guid_text(&self) -> Option<&str> {
        optional_string(&self.local, "guid")
    }

    pub fn set_name(&mut self, value: &str) {
        self.local
            .insert("name".to_owned(), JsonValue::String(value.to_owned()));
    }

    pub fn set_guid_text(&mut self, value: &str) {
        self.local
            .insert("guid".to_owned(), JsonValue::String(value.to_owned()));
    }

    /// Local JSON only. Inherited defaults are intentionally absent, matching
    /// `Profile::ToJson` after inheritance finalization.
    #[must_use]
    pub const fn to_json(&self) -> &JsonObject {
        &self.local
    }
}

/// Safe Rust owner for the deterministic portion of profile duplication.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct ProfileDuplicationSettings {
    defaults: JsonObject,
    profiles: Vec<DuplicableProfile>,
}

impl ProfileDuplicationSettings {
    /// Parses modern `profiles.defaults` plus `profiles.list` without flattening
    /// inherited values into each profile's local JSON layer.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the settings shape is invalid.
    pub fn from_json(input: &str) -> Result<Self, ProfileParseError> {
        let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
        let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
        let profiles_object = match JsonMember::from_object(root, "profiles") {
            JsonMember::Missing | JsonMember::Null => return Ok(Self::default()),
            JsonMember::Value(JsonValue::Object(value)) => value,
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        };

        let defaults = match JsonMember::from_object(profiles_object, "defaults") {
            JsonMember::Missing | JsonMember::Null => JsonObject::new(),
            JsonMember::Value(JsonValue::Object(value)) => value.clone(),
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        };

        let values = match JsonMember::from_object(profiles_object, "list") {
            JsonMember::Missing | JsonMember::Null => {
                return Ok(Self {
                    defaults,
                    profiles: Vec::new(),
                });
            }
            JsonMember::Value(JsonValue::Array(values)) => values,
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
        };

        let profiles = values
            .iter()
            .map(|value| {
                let local = value
                    .as_object()
                    .cloned()
                    .ok_or(ProfileParseError::ExpectedObject)?;
                Ok(DuplicableProfile {
                    defaults: defaults.clone(),
                    local,
                })
            })
            .collect::<Result<Vec<_>, ProfileParseError>>()?;

        Ok(Self { defaults, profiles })
    }

    #[must_use]
    pub fn profiles(&self) -> &[DuplicableProfile] {
        &self.profiles
    }

    /// Duplicates the deterministic profile settings while leaving the fresh
    /// random GUID and localized copy name to their native/UI boundary. The
    /// Microsoft unit test overwrites those fields before comparing JSON.
    #[must_use]
    pub fn duplicate_profile(&self, index: usize) -> Option<DuplicableProfile> {
        let source = self.profiles.get(index)?;
        let mut local = source.local.clone();
        local.remove("name");
        local.remove("guid");
        Some(DuplicableProfile {
            defaults: self.defaults.clone(),
            local,
        })
    }
}

fn optional_string<'a>(object: &'a JsonObject, key: &str) -> Option<&'a str> {
    match JsonMember::from_object(object, key) {
        JsonMember::Value(JsonValue::String(value)) => Some(value.as_str()),
        _ => None,
    }
}

fn font_size_from_object(object: &JsonObject) -> Result<Option<i32>, ProfileParseError> {
    let font = match JsonMember::from_object(object, "font") {
        JsonMember::Missing | JsonMember::Null => return Ok(None),
        JsonMember::Value(JsonValue::Object(value)) => value,
        JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
    };

    match JsonMember::from_object(font, "size") {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::Number(value)) => parse_i32(*value).map(Some),
        JsonMember::Value(_) => Err(ProfileParseError::InvalidInteger),
    }
}

fn parse_i32(value: f64) -> Result<i32, ProfileParseError> {
    if value.fract() != 0.0 || value < f64::from(i32::MIN) || value > f64::from(i32::MAX) {
        return Err(ProfileParseError::InvalidInteger);
    }
    value
        .to_string()
        .parse::<i32>()
        .map_err(|_| ProfileParseError::InvalidInteger)
}
