//! Portable theme-model semantics from `SettingsModel`.
//!
//! This module owns deterministic theme parsing and selection behavior while
//! XAML/WinRT projection remains at the platform boundary.

use std::collections::BTreeMap;

use crate::settings_json::{self, JsonMember, JsonObject, JsonValue};

/// RGBA color value used by theme settings.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    #[must_use]
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }

    #[must_use]
    pub const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }
}

/// Portable equivalent of XAML `ElementTheme`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ElementTheme {
    #[default]
    Default,
    Light,
    Dark,
}

/// Theme-color source kind.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ThemeColorType {
    Color,
    Accent,
    TerminalBackground,
}

/// Theme color preserving its semantic source kind.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ThemeColor {
    color_type: ThemeColorType,
    color: Option<Color>,
}

impl ThemeColor {
    #[must_use]
    pub const fn color_type(&self) -> ThemeColorType {
        self.color_type
    }

    #[must_use]
    pub const fn color(&self) -> Option<Color> {
        self.color
    }
}

/// Theme settings applied to the tab row.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct TabRowTheme {
    background: Option<ThemeColor>,
    unfocused_background: Option<ThemeColor>,
}

impl TabRowTheme {
    #[must_use]
    pub const fn background(&self) -> Option<ThemeColor> {
        self.background
    }

    #[must_use]
    pub const fn unfocused_background(&self) -> Option<ThemeColor> {
        self.unfocused_background
    }
}

/// Theme settings applied to the window surface.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct WindowTheme {
    requested_theme: ElementTheme,
    use_mica: bool,
}

impl WindowTheme {
    #[must_use]
    pub const fn requested_theme(&self) -> ElementTheme {
        self.requested_theme
    }

    #[must_use]
    pub const fn use_mica(&self) -> bool {
        self.use_mica
    }
}

/// Portable theme owner.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Theme {
    name: String,
    tab_row: Option<TabRowTheme>,
    window: Option<WindowTheme>,
}

impl Theme {
    /// Parses a serialized theme object.
    ///
    /// Missing or explicit-null sub-objects remain absent, matching Microsoft's
    /// `Theme::FromJson` behavior.
    ///
    /// # Errors
    ///
    /// Returns [`ThemeParseError`] when the theme document or a supported value
    /// is malformed.
    pub fn from_json(input: &str) -> Result<Self, ThemeParseError> {
        let value = settings_json::parse(input).map_err(|_| ThemeParseError::InvalidJson)?;
        let object = value.as_object().ok_or(ThemeParseError::InvalidObject)?;
        Self::from_object(object)
    }

    fn from_object(object: &JsonObject) -> Result<Self, ThemeParseError> {
        let name = required_string(object, "name")?.to_owned();
        let tab_row = optional_object(object, "tabRow")?
            .map(parse_tab_row)
            .transpose()?;
        let window = optional_object(object, "window")?
            .map(parse_window)
            .transpose()?;

        Ok(Self {
            name,
            tab_row,
            window,
        })
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub const fn tab_row(&self) -> Option<&TabRowTheme> {
        self.tab_row.as_ref()
    }

    #[must_use]
    pub const fn window(&self) -> Option<&WindowTheme> {
        self.window.as_ref()
    }

    #[must_use]
    pub fn requested_theme(&self) -> ElementTheme {
        self.window
            .as_ref()
            .map_or(ElementTheme::Default, WindowTheme::requested_theme)
    }

    fn system() -> Self {
        Self {
            name: "system".to_owned(),
            tab_row: None,
            window: None,
        }
    }
}

/// Settings warning produced by deterministic theme selection.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SettingsLoadWarning {
    UnknownTheme,
}

/// Theme collection plus current-theme selection semantics.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ThemeSettings {
    themes: BTreeMap<String, Theme>,
    current_theme_name: String,
    warnings: Vec<SettingsLoadWarning>,
}

impl ThemeSettings {
    /// Parses the theme-related subset of a user settings object.
    ///
    /// # Errors
    ///
    /// Returns [`ThemeParseError`] when the settings document, themes array, or
    /// any contained theme is malformed.
    pub fn from_user_settings_json(input: &str) -> Result<Self, ThemeParseError> {
        let value = settings_json::parse(input).map_err(|_| ThemeParseError::InvalidJson)?;
        let object = value.as_object().ok_or(ThemeParseError::InvalidObject)?;

        let mut themes = BTreeMap::new();
        match JsonMember::from_object(object, "themes") {
            JsonMember::Missing | JsonMember::Null => {}
            JsonMember::Value(JsonValue::Array(values)) => {
                for value in values {
                    let theme_object = value.as_object().ok_or(ThemeParseError::InvalidObject)?;
                    let theme = Theme::from_object(theme_object)?;
                    themes.insert(theme.name.clone(), theme);
                }
            }
            JsonMember::Value(_) => return Err(ThemeParseError::InvalidArray),
        }

        let requested = optional_string(object, "theme")?
            .unwrap_or("system")
            .to_owned();
        let known_builtin = matches!(requested.as_str(), "system" | "light" | "dark");
        let valid = known_builtin || themes.contains_key(&requested);
        let warnings = if valid {
            Vec::new()
        } else {
            vec![SettingsLoadWarning::UnknownTheme]
        };

        Ok(Self {
            themes,
            current_theme_name: if valid {
                requested
            } else {
                "system".to_owned()
            },
            warnings,
        })
    }

    #[must_use]
    pub fn theme(&self, name: &str) -> Option<&Theme> {
        self.themes.get(name)
    }

    #[must_use]
    pub fn warnings(&self) -> &[SettingsLoadWarning] {
        &self.warnings
    }

    #[must_use]
    pub fn current_theme(&self) -> Theme {
        self.themes
            .get(&self.current_theme_name)
            .cloned()
            .unwrap_or_else(Theme::system)
    }
}

/// Parse failures for the portable theme slice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ThemeParseError {
    InvalidJson,
    MissingName,
    InvalidString,
    InvalidObject,
    InvalidArray,
    InvalidBoolean,
    InvalidColor,
    InvalidElementTheme,
}

fn parse_tab_row(object: &JsonObject) -> Result<TabRowTheme, ThemeParseError> {
    Ok(TabRowTheme {
        background: theme_color_member(object, "background")?,
        unfocused_background: theme_color_member(object, "unfocusedBackground")?,
    })
}

fn parse_window(object: &JsonObject) -> Result<WindowTheme, ThemeParseError> {
    let requested_theme = match optional_string(object, "applicationTheme")? {
        None => ElementTheme::Default,
        Some(value) if value.eq_ignore_ascii_case("light") => ElementTheme::Light,
        Some(value) if value.eq_ignore_ascii_case("dark") => ElementTheme::Dark,
        Some(value) if value.eq_ignore_ascii_case("system") => ElementTheme::Default,
        Some(_) => return Err(ThemeParseError::InvalidElementTheme),
    };

    Ok(WindowTheme {
        requested_theme,
        use_mica: optional_bool(object, "useMica")?.unwrap_or(false),
    })
}

fn theme_color_member(
    object: &JsonObject,
    key: &str,
) -> Result<Option<ThemeColor>, ThemeParseError> {
    let value = match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => return Ok(None),
        JsonMember::Value(JsonValue::String(value)) => value,
        JsonMember::Value(_) => return Err(ThemeParseError::InvalidColor),
    };

    let color = match value.as_str() {
        "accent" => ThemeColor {
            color_type: ThemeColorType::Accent,
            color: None,
        },
        "terminalBackground" => ThemeColor {
            color_type: ThemeColorType::TerminalBackground,
            color: None,
        },
        _ => ThemeColor {
            color_type: ThemeColorType::Color,
            color: Some(parse_hex_color(value)?),
        },
    };
    Ok(Some(color))
}

fn parse_hex_color(value: &str) -> Result<Color, ThemeParseError> {
    let digits = value
        .strip_prefix('#')
        .ok_or(ThemeParseError::InvalidColor)?;
    match digits.len() {
        6 => Ok(Color::rgb(
            parse_hex_byte(&digits[0..2])?,
            parse_hex_byte(&digits[2..4])?,
            parse_hex_byte(&digits[4..6])?,
        )),
        8 => Ok(Color::rgba(
            parse_hex_byte(&digits[0..2])?,
            parse_hex_byte(&digits[2..4])?,
            parse_hex_byte(&digits[4..6])?,
            parse_hex_byte(&digits[6..8])?,
        )),
        _ => Err(ThemeParseError::InvalidColor),
    }
}

fn parse_hex_byte(value: &str) -> Result<u8, ThemeParseError> {
    u8::from_str_radix(value, 16).map_err(|_| ThemeParseError::InvalidColor)
}

fn required_string<'a>(object: &'a JsonObject, key: &str) -> Result<&'a str, ThemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Value(JsonValue::String(value)) => Ok(value),
        JsonMember::Missing | JsonMember::Null => Err(ThemeParseError::MissingName),
        JsonMember::Value(_) => Err(ThemeParseError::InvalidString),
    }
}

fn optional_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a str>, ThemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::String(value)) => Ok(Some(value)),
        JsonMember::Value(_) => Err(ThemeParseError::InvalidString),
    }
}

fn optional_bool(object: &JsonObject, key: &str) -> Result<Option<bool>, ThemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::Bool(value)) => Ok(Some(*value)),
        JsonMember::Value(_) => Err(ThemeParseError::InvalidBoolean),
    }
}

fn optional_object<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a JsonObject>, ThemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(None),
        JsonMember::Value(JsonValue::Object(value)) => Ok(Some(value)),
        JsonMember::Value(_) => Err(ThemeParseError::InvalidObject),
    }
}
