//! Portable `NewTabMenu` settings semantics.
//!
//! This slice intentionally owns only the deterministic menu-model behavior
//! exercised by Microsoft's `NewTabMenuTests`. Broader settings JSON layering is
//! added by later `SettingsModel` slices.

use crate::settings_json::{self, JsonMember, JsonValue};

/// New-tab menu entry kinds from `NewTabMenuEntry.idl`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NewTabMenuEntryType {
    Invalid,
    Profile,
    Separator,
    Folder,
    RemainingProfiles,
    MatchProfiles,
    Action,
}

/// Portable new-tab menu entry.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NewTabMenuEntry {
    entry_type: NewTabMenuEntryType,
}

impl NewTabMenuEntry {
    fn new(entry_type: NewTabMenuEntryType) -> Self {
        Self { entry_type }
    }

    #[must_use]
    pub const fn entry_type(&self) -> NewTabMenuEntryType {
        self.entry_type
    }
}

/// Deterministic projection of `WindowSettingsDefaults().NewTabMenu()`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct NewTabMenuSettings {
    entries: Vec<NewTabMenuEntry>,
    warnings: Vec<String>,
}

impl NewTabMenuSettings {
    /// Parses only the `newTabMenu` fragment needed by this migration slice.
    ///
    /// Microsoft defaults an absent `newTabMenu` property to one
    /// `RemainingProfiles` entry. A present array is preserved as provided; in
    /// particular, a folder with no name or child entries remains a valid entry.
    ///
    /// # Errors
    ///
    /// Returns [`NewTabMenuParseError`] when the settings document is malformed,
    /// `newTabMenu` is not an array, or an entry has an unsupported type.
    pub fn from_user_settings_json(input: &str) -> Result<Self, NewTabMenuParseError> {
        let document =
            settings_json::parse(input).map_err(|_| NewTabMenuParseError::InvalidJson)?;
        let object = document
            .as_object()
            .ok_or(NewTabMenuParseError::ExpectedObject)?;

        let array = match JsonMember::from_object(object, "newTabMenu") {
            JsonMember::Missing => {
                return Ok(Self {
                    entries: vec![NewTabMenuEntry::new(NewTabMenuEntryType::RemainingProfiles)],
                    warnings: Vec::new(),
                });
            }
            JsonMember::Null => return Err(NewTabMenuParseError::ExpectedArray),
            JsonMember::Value(value) => value
                .as_array()
                .ok_or(NewTabMenuParseError::ExpectedArray)?,
        };

        let mut entries = Vec::with_capacity(array.len());
        for value in array {
            let entry = value
                .as_object()
                .ok_or(NewTabMenuParseError::ExpectedObject)?;
            entries.push(NewTabMenuEntry::new(parse_entry_type(entry)?));
        }

        Ok(Self {
            entries,
            warnings: Vec::new(),
        })
    }

    #[must_use]
    pub fn entries(&self) -> &[NewTabMenuEntry] {
        &self.entries
    }

    #[must_use]
    pub fn warnings(&self) -> &[String] {
        &self.warnings
    }
}

/// Parse failures for the deliberately narrow new-tab-menu settings slice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NewTabMenuParseError {
    InvalidJson,
    ExpectedObject,
    ExpectedArray,
    MissingType,
    UnknownType,
}

fn parse_entry_type(
    object: &settings_json::JsonObject,
) -> Result<NewTabMenuEntryType, NewTabMenuParseError> {
    let value = match JsonMember::from_object(object, "type") {
        JsonMember::Value(JsonValue::String(value)) => value.as_str(),
        JsonMember::Missing | JsonMember::Null | JsonMember::Value(_) => {
            return Err(NewTabMenuParseError::MissingType);
        }
    };

    match value {
        "profile" => Ok(NewTabMenuEntryType::Profile),
        "separator" => Ok(NewTabMenuEntryType::Separator),
        "folder" => Ok(NewTabMenuEntryType::Folder),
        "remainingProfiles" => Ok(NewTabMenuEntryType::RemainingProfiles),
        "matchProfiles" => Ok(NewTabMenuEntryType::MatchProfiles),
        "action" => Ok(NewTabMenuEntryType::Action),
        _ => Err(NewTabMenuParseError::UnknownType),
    }
}
