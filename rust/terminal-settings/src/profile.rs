//! Portable profile inheritance semantics from `SettingsModel`.
//!
//! This slice owns deterministic fallback, local-ownership (`HasXxx`), clear,
//! nullable icon layering, GUID parsing, appearance layering and profile
//! environment-map behavior. `WinRT` projection and the broader profile surface
//! remain outside this owner until their Microsoft contracts migrate.

use std::{collections::BTreeMap, fmt};

use crate::{
    color_scheme::Color,
    settings_json::{self, JsonMember, JsonObject, JsonValue},
};

const DEFAULT_HISTORY_SIZE: i32 = 9001;
const DEFAULT_SNAP_ON_INPUT: bool = true;

#[derive(Debug, Clone, PartialEq, Eq)]
struct LayeredSetting<T> {
    inherited: T,
    local: Option<T>,
}

impl<T> LayeredSetting<T> {
    const fn new(inherited: T) -> Self {
        Self {
            inherited,
            local: None,
        }
    }

    fn set(&mut self, value: T) {
        self.local = Some(value);
    }

    fn clear(&mut self) {
        self.local = None;
    }

    const fn has_local(&self) -> bool {
        self.local.is_some()
    }
}

impl<T> LayeredSetting<T>
where
    T: Clone,
{
    fn resolved(&self) -> T {
        self.local.as_ref().unwrap_or(&self.inherited).clone()
    }

    fn inherited_from(parent: &Self) -> Self {
        Self::new(parent.resolved())
    }
}

/// Strict braced GUID identity used by `Profile::FromJson`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ProfileGuid([u8; 16]);

impl ProfileGuid {
    /// Parses the exact `{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}` format accepted
    /// by Microsoft's profile JSON contract.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError::InvalidGuid`] for missing braces, missing
    /// hyphens, non-hex digits or any other GUID presentation format.
    pub fn parse(value: &str) -> Result<Self, ProfileParseError> {
        let bytes = value.as_bytes();
        if bytes.len() != 38 || bytes[0] != b'{' || bytes[37] != b'}' {
            return Err(ProfileParseError::InvalidGuid);
        }

        let body = &value[1..37];
        for index in [8, 13, 18, 23] {
            if body.as_bytes()[index] != b'-' {
                return Err(ProfileParseError::InvalidGuid);
            }
        }

        let mut compact = [0_u8; 32];
        let mut compact_index = 0;
        for (index, byte) in body.bytes().enumerate() {
            if matches!(index, 8 | 13 | 18 | 23) {
                continue;
            }
            if !byte.is_ascii_hexdigit() {
                return Err(ProfileParseError::InvalidGuid);
            }
            compact[compact_index] = byte;
            compact_index += 1;
        }
        if compact_index != compact.len() {
            return Err(ProfileParseError::InvalidGuid);
        }

        let mut guid = [0_u8; 16];
        for (index, slot) in guid.iter_mut().enumerate() {
            *slot = parse_hex_pair(compact[index * 2], compact[index * 2 + 1])?;
        }
        Ok(Self(guid))
    }

    #[must_use]
    pub const fn is_zero(self) -> bool {
        let bytes = self.0;
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

impl fmt::Display for ProfileGuid {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        let b = self.0;
        write!(
            formatter,
            "{{{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}}}",
            b[0],
            b[1],
            b[2],
            b[3],
            b[4],
            b[5],
            b[6],
            b[7],
            b[8],
            b[9],
            b[10],
            b[11],
            b[12],
            b[13],
            b[14],
            b[15]
        )
    }
}

/// Safe Rust owner for the currently migrated profile surface.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Profile {
    name: Option<String>,
    guid: LayeredSetting<Option<ProfileGuid>>,
    history_size: LayeredSetting<i32>,
    snap_on_input: LayeredSetting<bool>,
    tab_title: LayeredSetting<String>,
    icon: LayeredSetting<String>,
    foreground: LayeredSetting<Option<Color>>,
    background: LayeredSetting<Option<Color>>,
    selection_background: LayeredSetting<Option<Color>>,
    starting_directory: LayeredSetting<String>,
    environment_variables: BTreeMap<String, String>,
}

impl Default for Profile {
    fn default() -> Self {
        Self {
            name: None,
            guid: LayeredSetting::new(None),
            history_size: LayeredSetting::new(DEFAULT_HISTORY_SIZE),
            snap_on_input: LayeredSetting::new(DEFAULT_SNAP_ON_INPUT),
            tab_title: LayeredSetting::new(String::new()),
            icon: LayeredSetting::new(String::new()),
            foreground: LayeredSetting::new(None),
            background: LayeredSetting::new(None),
            selection_background: LayeredSetting::new(None),
            starting_directory: LayeredSetting::new(String::new()),
            environment_variables: BTreeMap::new(),
        }
    }
}

impl Profile {
    /// Parses one profile layer on top of Windows Terminal's built-in defaults.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the JSON document or one of the
    /// migrated profile settings has the wrong shape or type.
    pub fn from_json(input: &str) -> Result<Self, ProfileParseError> {
        let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
        let object = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
        let mut profile = Self::default();
        profile.layer_object(object)?;
        Ok(profile)
    }

    /// Creates a child whose values inherit from the resolved parent but whose
    /// migrated settings are not locally owned until layered or explicitly set.
    #[must_use]
    pub fn create_child(&self) -> Self {
        Self {
            name: self.name.clone(),
            guid: LayeredSetting::inherited_from(&self.guid),
            history_size: LayeredSetting::inherited_from(&self.history_size),
            snap_on_input: LayeredSetting::inherited_from(&self.snap_on_input),
            tab_title: LayeredSetting::inherited_from(&self.tab_title),
            icon: LayeredSetting::inherited_from(&self.icon),
            foreground: LayeredSetting::inherited_from(&self.foreground),
            background: LayeredSetting::inherited_from(&self.background),
            selection_background: LayeredSetting::inherited_from(&self.selection_background),
            starting_directory: LayeredSetting::inherited_from(&self.starting_directory),
            environment_variables: self.environment_variables.clone(),
        }
    }

    /// Layers migrated settings from one JSON profile object onto this layer.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the JSON document or a migrated
    /// setting has the wrong shape or type.
    pub fn layer_json(&mut self, input: &str) -> Result<(), ProfileParseError> {
        let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
        let object = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
        self.layer_object(object)
    }

    fn layer_object(&mut self, object: &JsonObject) -> Result<(), ProfileParseError> {
        match JsonMember::from_object(object, "name") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::String(value)) => self.name = Some(value.clone()),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
        }

        match JsonMember::from_object(object, "guid") {
            JsonMember::Missing => {}
            JsonMember::Null => self.guid.set(None),
            JsonMember::Value(JsonValue::String(value)) => {
                self.guid.set(Some(ProfileGuid::parse(value)?));
            }
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidGuid),
        }

        match JsonMember::from_object(object, "historySize") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Number(value)) => {
                self.history_size.set(parse_i32(*value)?);
            }
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidInteger),
        }

        match JsonMember::from_object(object, "snapOnInput") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Bool(value)) => self.snap_on_input.set(*value),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidBoolean),
        }

        match JsonMember::from_object(object, "tabTitle") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::String(value)) => self.tab_title.set(value.clone()),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
        }

        match JsonMember::from_object(object, "icon") {
            JsonMember::Missing => {}
            JsonMember::Null => self.icon.set(String::new()),
            JsonMember::Value(JsonValue::String(value)) => self.icon.set(value.clone()),
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
        }

        layer_optional_color(object, "foreground", &mut self.foreground)?;
        layer_optional_color(object, "background", &mut self.background)?;
        layer_optional_color(
            object,
            "selectionBackground",
            &mut self.selection_background,
        )?;

        match JsonMember::from_object(object, "startingDirectory") {
            JsonMember::Missing => {}
            JsonMember::Null => self.starting_directory.set(String::new()),
            JsonMember::Value(JsonValue::String(value)) => {
                self.starting_directory.set(value.clone());
            }
            JsonMember::Value(_) => return Err(ProfileParseError::InvalidString),
        }

        match JsonMember::from_object(object, "environment") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Object(environment)) => {
                let mut values = BTreeMap::new();
                for (name, value) in environment {
                    let JsonValue::String(value) = value else {
                        return Err(ProfileParseError::InvalidEnvironmentVariable);
                    };
                    values.insert(name.clone(), value.clone());
                }
                self.environment_variables = values;
            }
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        }

        Ok(())
    }

    #[must_use]
    pub fn name(&self) -> Option<&str> {
        self.name.as_deref()
    }

    #[must_use]
    pub fn guid(&self) -> Option<ProfileGuid> {
        self.guid.resolved()
    }

    #[must_use]
    pub fn has_guid(&self) -> bool {
        self.guid().is_some()
    }

    #[must_use]
    pub fn history_size(&self) -> i32 {
        self.history_size.resolved()
    }

    #[must_use]
    pub const fn has_history_size(&self) -> bool {
        self.history_size.has_local()
    }

    pub fn clear_history_size(&mut self) {
        self.history_size.clear();
    }

    #[must_use]
    pub fn snap_on_input(&self) -> bool {
        self.snap_on_input.resolved()
    }

    #[must_use]
    pub const fn has_snap_on_input(&self) -> bool {
        self.snap_on_input.has_local()
    }

    #[must_use]
    pub fn tab_title(&self) -> String {
        self.tab_title.resolved()
    }

    #[must_use]
    pub const fn has_tab_title(&self) -> bool {
        self.tab_title.has_local()
    }

    pub fn clear_tab_title(&mut self) {
        self.tab_title.clear();
    }

    #[must_use]
    pub fn icon_path(&self) -> String {
        self.icon.resolved()
    }

    #[must_use]
    pub fn foreground(&self) -> Option<Color> {
        self.foreground.resolved()
    }

    #[must_use]
    pub fn background(&self) -> Option<Color> {
        self.background.resolved()
    }

    #[must_use]
    pub fn selection_background(&self) -> Option<Color> {
        self.selection_background.resolved()
    }

    #[must_use]
    pub fn starting_directory(&self) -> String {
        self.starting_directory.resolved()
    }

    #[must_use]
    pub const fn environment_variables(&self) -> &BTreeMap<String, String> {
        &self.environment_variables
    }
}

/// Profiles.defaults plus resolved profile children for the migrated settings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProfileSettings {
    defaults: Profile,
    profiles: Vec<Profile>,
}

impl ProfileSettings {
    /// Parses the `profiles.defaults` / `profiles.list` settings shape used by
    /// Microsoft's profile inheritance contracts.
    ///
    /// # Errors
    ///
    /// Returns [`ProfileParseError`] when the document or migrated profile
    /// settings have an invalid shape or type.
    pub fn from_json(input: &str) -> Result<Self, ProfileParseError> {
        let value = settings_json::parse(input).map_err(|_| ProfileParseError::InvalidJson)?;
        let root = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;

        let profiles_object = match JsonMember::from_object(root, "profiles") {
            JsonMember::Value(JsonValue::Object(value)) => value,
            JsonMember::Missing | JsonMember::Null => {
                return Ok(Self {
                    defaults: Profile::default(),
                    profiles: Vec::new(),
                });
            }
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        };

        let mut defaults = Profile::default();
        match JsonMember::from_object(profiles_object, "defaults") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Object(value)) => defaults.layer_object(value)?,
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedObject),
        }

        let mut profiles = Vec::new();
        match JsonMember::from_object(profiles_object, "list") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Array(values)) => {
                profiles.reserve(values.len());
                for value in values {
                    let object = value.as_object().ok_or(ProfileParseError::ExpectedObject)?;
                    let mut profile = defaults.create_child();
                    profile.layer_object(object)?;
                    profiles.push(profile);
                }
            }
            JsonMember::Value(_) => return Err(ProfileParseError::ExpectedArray),
        }

        Ok(Self { defaults, profiles })
    }

    #[must_use]
    pub const fn defaults(&self) -> &Profile {
        &self.defaults
    }

    #[must_use]
    pub fn profiles(&self) -> &[Profile] {
        &self.profiles
    }
}

/// Parse failures for the migrated profile slice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ProfileParseError {
    InvalidJson,
    ExpectedObject,
    ExpectedArray,
    InvalidInteger,
    InvalidBoolean,
    InvalidString,
    InvalidGuid,
    InvalidColor,
    InvalidEnvironmentVariable,
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

fn layer_optional_color(
    object: &JsonObject,
    key: &str,
    setting: &mut LayeredSetting<Option<Color>>,
) -> Result<(), ProfileParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing => {}
        JsonMember::Null => setting.set(None),
        JsonMember::Value(JsonValue::String(value)) => setting.set(Some(parse_color(value)?)),
        JsonMember::Value(_) => return Err(ProfileParseError::InvalidColor),
    }
    Ok(())
}

fn parse_color(value: &str) -> Result<Color, ProfileParseError> {
    let digits = value
        .strip_prefix('#')
        .ok_or(ProfileParseError::InvalidColor)?;
    if digits.len() != 6 || !digits.is_ascii() {
        return Err(ProfileParseError::InvalidColor);
    }
    Ok(Color::rgb(
        u8::from_str_radix(&digits[0..2], 16).map_err(|_| ProfileParseError::InvalidColor)?,
        u8::from_str_radix(&digits[2..4], 16).map_err(|_| ProfileParseError::InvalidColor)?,
        u8::from_str_radix(&digits[4..6], 16).map_err(|_| ProfileParseError::InvalidColor)?,
    ))
}

fn parse_hex_pair(high: u8, low: u8) -> Result<u8, ProfileParseError> {
    Ok(hex_nibble(high)? << 4 | hex_nibble(low)?)
}

const fn hex_nibble(value: u8) -> Result<u8, ProfileParseError> {
    match value {
        b'0'..=b'9' => Ok(value - b'0'),
        b'a'..=b'f' => Ok(value - b'a' + 10),
        b'A'..=b'F' => Ok(value - b'A' + 10),
        _ => Err(ProfileParseError::InvalidGuid),
    }
}
