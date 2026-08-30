//! Portable color-scheme semantics from `SettingsModel`.
//!
//! This owner covers deterministic parsing/round-tripping, layered scheme
//! ownership, user-collision preservation and profile-reference retargeting
//! exercised by Microsoft's `ColorSchemeTests`.

use std::collections::{BTreeMap, BTreeSet};

use crate::settings_json::{self, JsonMember, JsonObject, JsonValue};

/// RGBA color used by the portable settings model.
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
}

const DEFAULT_FOREGROUND: Color = Color::rgb(0xc0, 0xc0, 0xc0);
const DEFAULT_BACKGROUND: Color = Color::rgb(0x00, 0x00, 0x00);
const DEFAULT_CURSOR: Color = Color::rgb(0xff, 0xff, 0xff);
const DEFAULT_SCHEME_NAME: &str = "Campbell";

const TABLE_KEYS: [&str; 16] = [
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

/// Canonical safe Rust owner for a color scheme.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ColorScheme {
    name: String,
    foreground: Color,
    background: Color,
    selection_background: Color,
    cursor_color: Color,
    table: [Color; 16],
}

impl ColorScheme {
    /// Parses one serialized color-scheme object.
    ///
    /// # Errors
    ///
    /// Returns [`ColorSchemeParseError`] if the JSON is malformed, the root is
    /// not an object, a required member is missing/wrongly typed, or a color is
    /// not a six-digit `#RRGGBB` value.
    pub fn from_json(input: &str) -> Result<Self, ColorSchemeParseError> {
        let value = parse_document(input)?;
        let object = value
            .as_object()
            .ok_or(ColorSchemeParseError::ExpectedObject)?;
        Self::from_object(object)
    }

    fn from_object(object: &JsonObject) -> Result<Self, ColorSchemeParseError> {
        let name = required_string(object, "name")?.to_owned();
        let foreground = optional_color(object, "foreground", DEFAULT_FOREGROUND)?;
        let background = optional_color(object, "background", DEFAULT_BACKGROUND)?;
        let selection_background =
            optional_color(object, "selectionBackground", DEFAULT_FOREGROUND)?;
        let cursor_color = optional_color(object, "cursorColor", DEFAULT_CURSOR)?;

        let mut table = [Color::rgb(0, 0, 0); 16];
        for (index, key) in TABLE_KEYS.iter().enumerate() {
            table[index] = required_color(object, key)?;
        }

        Ok(Self {
            name,
            foreground,
            background,
            selection_background,
            cursor_color,
            table,
        })
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub const fn foreground(&self) -> Color {
        self.foreground
    }

    #[must_use]
    pub const fn background(&self) -> Color {
        self.background
    }

    #[must_use]
    pub const fn selection_background(&self) -> Color {
        self.selection_background
    }

    #[must_use]
    pub const fn cursor_color(&self) -> Color {
        self.cursor_color
    }

    #[must_use]
    pub const fn table(&self) -> &[Color; 16] {
        &self.table
    }

    /// Projects the owner back to the same typed JSON object shape consumed by
    /// Microsoft's `ColorScheme::ToJson` round-trip contract.
    #[must_use]
    pub fn to_json_value(&self) -> JsonValue {
        let mut object = JsonObject::new();
        object.insert("name".to_owned(), JsonValue::String(self.name.clone()));
        object.insert(
            "foreground".to_owned(),
            JsonValue::String(format_color(self.foreground)),
        );
        object.insert(
            "background".to_owned(),
            JsonValue::String(format_color(self.background)),
        );
        object.insert(
            "selectionBackground".to_owned(),
            JsonValue::String(format_color(self.selection_background)),
        );
        object.insert(
            "cursorColor".to_owned(),
            JsonValue::String(format_color(self.cursor_color)),
        );
        for (key, color) in TABLE_KEYS.iter().zip(self.table) {
            object.insert((*key).to_owned(), JsonValue::String(format_color(color)));
        }
        JsonValue::Object(object)
    }

    fn equivalent_for_settings_merge(&self, other: &Self) -> bool {
        self.table == other.table
            && self.background == other.background
            && self.foreground == other.foreground
    }

    fn rename(&mut self, name: String) {
        self.name = name;
    }
}

/// Layered color-scheme collection from inbox defaults followed by user settings.
#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct ColorSchemeCollection {
    schemes: BTreeMap<String, ColorScheme>,
}

impl ColorSchemeCollection {
    /// Layers non-colliding `schemes` arrays from inbox and user settings.
    ///
    /// # Errors
    ///
    /// Returns [`ColorSchemeParseError`] for malformed settings/schemes or when
    /// two layers collide by name. Full collision ownership is exposed through
    /// [`ColorSchemeSettings::from_layers`].
    pub fn from_inbox_and_user_json(
        inbox: &str,
        user: &str,
    ) -> Result<Self, ColorSchemeParseError> {
        let mut result = Self::default();
        result.layer_document(inbox)?;
        result.layer_document(user)?;
        Ok(result)
    }

    fn layer_document(&mut self, input: &str) -> Result<(), ColorSchemeParseError> {
        for scheme in parse_schemes(input)? {
            if self.schemes.contains_key(scheme.name()) {
                return Err(ColorSchemeParseError::CollisionRequiresRetargeting);
            }
            self.schemes.insert(scheme.name.clone(), scheme);
        }
        Ok(())
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.schemes.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.schemes.is_empty()
    }

    #[must_use]
    pub fn get(&self, name: &str) -> Option<&ColorScheme> {
        self.schemes.get(name)
    }
}

/// Origin of a layered color scheme.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OriginTag {
    InBox,
    Fragment,
    User,
}

/// A color scheme plus the layer that owns it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OwnedColorScheme {
    scheme: ColorScheme,
    origin: OriginTag,
}

impl OwnedColorScheme {
    #[must_use]
    pub const fn scheme(&self) -> &ColorScheme {
        &self.scheme
    }

    #[must_use]
    pub const fn origin(&self) -> OriginTag {
        self.origin
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ReferenceSource {
    Fallback,
    Defaults,
    DefaultAppearance,
    FragmentParent,
    Explicit,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct SchemeSlot {
    value: String,
    explicit: bool,
    source: ReferenceSource,
}

impl SchemeSlot {
    fn fallback() -> Self {
        Self {
            value: DEFAULT_SCHEME_NAME.to_owned(),
            explicit: false,
            source: ReferenceSource::Fallback,
        }
    }

    fn inherited(value: &str, source: ReferenceSource) -> Self {
        Self {
            value: value.to_owned(),
            explicit: false,
            source,
        }
    }

    fn set(&mut self, value: &str, source: ReferenceSource, explicit: bool) {
        self.value.clear();
        self.value.push_str(value);
        self.source = source;
        self.explicit = explicit;
    }
}

/// Effective light/dark scheme references together with local-override state.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AppearanceReferences {
    light: SchemeSlot,
    dark: SchemeSlot,
}

impl AppearanceReferences {
    fn fallback() -> Self {
        Self {
            light: SchemeSlot::fallback(),
            dark: SchemeSlot::fallback(),
        }
    }

    fn inherited_from(parent: &Self, source: ReferenceSource) -> Self {
        Self {
            light: SchemeSlot::inherited(&parent.light.value, source),
            dark: SchemeSlot::inherited(&parent.dark.value, source),
        }
    }

    #[must_use]
    pub fn light_name(&self) -> &str {
        &self.light.value
    }

    #[must_use]
    pub fn dark_name(&self) -> &str {
        &self.dark.value
    }

    #[must_use]
    pub const fn has_light_name(&self) -> bool {
        self.light.explicit
    }

    #[must_use]
    pub const fn has_dark_name(&self) -> bool {
        self.dark.explicit
    }
}

/// Profile references required by color-scheme rename/fixup behavior.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProfileReferences {
    name: String,
    default_appearance: AppearanceReferences,
    unfocused_appearance: AppearanceReferences,
}

impl ProfileReferences {
    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub const fn default_appearance(&self) -> &AppearanceReferences {
        &self.default_appearance
    }

    #[must_use]
    pub const fn unfocused_appearance(&self) -> &AppearanceReferences {
        &self.unfocused_appearance
    }
}

/// Product-level deterministic color-scheme layering owner.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ColorSchemeSettings {
    schemes: BTreeMap<String, OwnedColorScheme>,
    profile_defaults: AppearanceReferences,
    profiles: Vec<ProfileReferences>,
}

impl ColorSchemeSettings {
    /// Layers inbox, fragments and user settings with Microsoft's user-owned
    /// collision policy and profile-reference fixups.
    ///
    /// # Errors
    ///
    /// Returns [`ColorSchemeParseError`] if a supplied document, scheme, profile
    /// or color-scheme reference is malformed.
    pub fn from_layers(
        user: &str,
        inbox: &str,
        fragments: &[&str],
    ) -> Result<Self, ColorSchemeParseError> {
        let mut schemes = BTreeMap::new();
        for scheme in parse_schemes(inbox)? {
            schemes.insert(
                scheme.name.clone(),
                OwnedColorScheme {
                    scheme,
                    origin: OriginTag::InBox,
                },
            );
        }
        for fragment in fragments {
            for scheme in parse_schemes(fragment)? {
                schemes.insert(
                    scheme.name.clone(),
                    OwnedColorScheme {
                        scheme,
                        origin: OriginTag::Fragment,
                    },
                );
            }
        }

        let (profile_defaults, mut profiles) = parse_user_profiles(user)?;
        for fragment in fragments {
            profiles.extend(parse_fragment_profiles(fragment, &profile_defaults)?);
        }

        let mut result = Self {
            schemes,
            profile_defaults,
            profiles,
        };
        result.refresh_inherited();

        let user_schemes = parse_schemes(user)?;
        let reserved_user_names: BTreeSet<String> = user_schemes
            .iter()
            .map(|scheme| scheme.name.clone())
            .collect();

        for mut user_scheme in user_schemes {
            let original_name = user_scheme.name.clone();
            if let Some(existing) = result.schemes.get(&original_name) {
                if existing.scheme.equivalent_for_settings_merge(&user_scheme) {
                    continue;
                }
                let existing_origin = existing.origin;
                let modified_name = result.next_modified_name(&original_name, &reserved_user_names);
                result.retarget_for_user_collision(&original_name, &modified_name, existing_origin);
                user_scheme.rename(modified_name.clone());
                result.schemes.insert(
                    modified_name,
                    OwnedColorScheme {
                        scheme: user_scheme,
                        origin: OriginTag::User,
                    },
                );
            } else {
                result.schemes.insert(
                    original_name,
                    OwnedColorScheme {
                        scheme: user_scheme,
                        origin: OriginTag::User,
                    },
                );
            }
        }

        Ok(result)
    }

    /// Updates locally-owned profile/default references after an explicit scheme
    /// rename while preserving inheritance/local-override flags.
    pub fn update_scheme_references(&mut self, old_name: &str, new_name: &str) {
        retarget_explicit_slot(&mut self.profile_defaults.light, old_name, new_name);
        retarget_explicit_slot(&mut self.profile_defaults.dark, old_name, new_name);
        for profile in &mut self.profiles {
            retarget_explicit_slot(&mut profile.default_appearance.light, old_name, new_name);
            retarget_explicit_slot(&mut profile.default_appearance.dark, old_name, new_name);
            retarget_explicit_slot(&mut profile.unfocused_appearance.light, old_name, new_name);
            retarget_explicit_slot(&mut profile.unfocused_appearance.dark, old_name, new_name);
        }
        self.refresh_inherited();
    }

    #[must_use]
    pub fn scheme(&self, name: &str) -> Option<&OwnedColorScheme> {
        self.schemes.get(name)
    }

    #[must_use]
    pub fn scheme_count(&self) -> usize {
        self.schemes.len()
    }

    #[must_use]
    pub const fn profile_defaults(&self) -> &AppearanceReferences {
        &self.profile_defaults
    }

    #[must_use]
    pub fn profiles(&self) -> &[ProfileReferences] {
        &self.profiles
    }

    fn next_modified_name(&self, original: &str, reserved_user_names: &BTreeSet<String>) -> String {
        let first = format!("{original} (modified)");
        if !self.schemes.contains_key(&first) && !reserved_user_names.contains(&first) {
            return first;
        }
        for index in 2_u32.. {
            let candidate = format!("{original} (modified {index})");
            if !self.schemes.contains_key(&candidate) && !reserved_user_names.contains(&candidate) {
                return candidate;
            }
        }
        unreachable!("u32 candidate space cannot be exhausted by a settings document")
    }

    fn retarget_for_user_collision(
        &mut self,
        old_name: &str,
        new_name: &str,
        collided_origin: OriginTag,
    ) {
        retarget_default_collision_slot(
            &mut self.profile_defaults.light,
            old_name,
            new_name,
            collided_origin,
        );
        retarget_default_collision_slot(
            &mut self.profile_defaults.dark,
            old_name,
            new_name,
            collided_origin,
        );
        self.refresh_inherited();

        for profile in &mut self.profiles {
            retarget_collision_slot(&mut profile.default_appearance.light, old_name, new_name);
            retarget_collision_slot(&mut profile.default_appearance.dark, old_name, new_name);
        }
        self.refresh_inherited();

        for profile in &mut self.profiles {
            retarget_collision_slot(&mut profile.unfocused_appearance.light, old_name, new_name);
            retarget_collision_slot(&mut profile.unfocused_appearance.dark, old_name, new_name);
        }
        self.refresh_inherited();
    }

    fn refresh_inherited(&mut self) {
        for profile in &mut self.profiles {
            refresh_slot_from_parent(
                &mut profile.default_appearance.light,
                &self.profile_defaults.light,
                ReferenceSource::Defaults,
            );
            refresh_slot_from_parent(
                &mut profile.default_appearance.dark,
                &self.profile_defaults.dark,
                ReferenceSource::Defaults,
            );
            refresh_slot_from_parent(
                &mut profile.unfocused_appearance.light,
                &profile.default_appearance.light,
                ReferenceSource::DefaultAppearance,
            );
            refresh_slot_from_parent(
                &mut profile.unfocused_appearance.dark,
                &profile.default_appearance.dark,
                ReferenceSource::DefaultAppearance,
            );
        }
    }
}

/// Parse failures for the portable color-scheme slice.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ColorSchemeParseError {
    InvalidJson,
    ExpectedObject,
    ExpectedArray,
    MissingMember,
    InvalidString,
    InvalidColor,
    InvalidReference,
    CollisionRequiresRetargeting,
}

fn parse_user_profiles(
    input: &str,
) -> Result<(AppearanceReferences, Vec<ProfileReferences>), ColorSchemeParseError> {
    let document = parse_document(input)?;
    let root = document
        .as_object()
        .ok_or(ColorSchemeParseError::ExpectedObject)?;
    let mut defaults = AppearanceReferences::fallback();
    let mut profile_values: &[JsonValue] = &[];

    match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => {}
        JsonMember::Value(JsonValue::Array(values)) => profile_values = values,
        JsonMember::Value(JsonValue::Object(profiles)) => {
            if let JsonMember::Value(JsonValue::Object(default_object)) =
                JsonMember::from_object(profiles, "defaults")
            {
                apply_reference_member(
                    default_object,
                    &mut defaults,
                    ReferenceSource::Explicit,
                    true,
                )?;
            }
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => {}
                JsonMember::Value(JsonValue::Array(values)) => profile_values = values,
                JsonMember::Value(_) => return Err(ColorSchemeParseError::ExpectedArray),
            }
        }
        JsonMember::Value(_) => return Err(ColorSchemeParseError::ExpectedArray),
    }

    let mut profiles = Vec::with_capacity(profile_values.len());
    for value in profile_values {
        let object = value
            .as_object()
            .ok_or(ColorSchemeParseError::ExpectedObject)?;
        profiles.push(parse_profile(
            object,
            &defaults,
            ReferenceSource::Explicit,
            true,
        )?);
    }
    Ok((defaults, profiles))
}

fn parse_fragment_profiles(
    input: &str,
    defaults: &AppearanceReferences,
) -> Result<Vec<ProfileReferences>, ColorSchemeParseError> {
    let document = parse_document(input)?;
    let root = document
        .as_object()
        .ok_or(ColorSchemeParseError::ExpectedObject)?;
    let values = match JsonMember::from_object(root, "profiles") {
        JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => values,
        JsonMember::Value(JsonValue::Object(profiles)) => {
            match JsonMember::from_object(profiles, "list") {
                JsonMember::Missing | JsonMember::Null => return Ok(Vec::new()),
                JsonMember::Value(JsonValue::Array(values)) => values,
                JsonMember::Value(_) => return Err(ColorSchemeParseError::ExpectedArray),
            }
        }
        JsonMember::Value(_) => return Err(ColorSchemeParseError::ExpectedArray),
    };

    values
        .iter()
        .map(|value| {
            let object = value
                .as_object()
                .ok_or(ColorSchemeParseError::ExpectedObject)?;
            parse_profile(object, defaults, ReferenceSource::FragmentParent, false)
        })
        .collect()
}

fn parse_profile(
    object: &JsonObject,
    defaults: &AppearanceReferences,
    local_source: ReferenceSource,
    local_is_explicit: bool,
) -> Result<ProfileReferences, ColorSchemeParseError> {
    let name = required_string(object, "name")?.to_owned();
    let mut default_appearance =
        AppearanceReferences::inherited_from(defaults, ReferenceSource::Defaults);
    apply_reference_member(
        object,
        &mut default_appearance,
        local_source,
        local_is_explicit,
    )?;

    let mut unfocused_appearance = AppearanceReferences::inherited_from(
        &default_appearance,
        ReferenceSource::DefaultAppearance,
    );
    match JsonMember::from_object(object, "unfocusedAppearance") {
        JsonMember::Missing | JsonMember::Null => {}
        JsonMember::Value(JsonValue::Object(unfocused)) => {
            apply_reference_member(
                unfocused,
                &mut unfocused_appearance,
                local_source,
                local_is_explicit,
            )?;
        }
        JsonMember::Value(_) => return Err(ColorSchemeParseError::ExpectedObject),
    }

    Ok(ProfileReferences {
        name,
        default_appearance,
        unfocused_appearance,
    })
}

fn apply_reference_member(
    object: &JsonObject,
    appearance: &mut AppearanceReferences,
    source: ReferenceSource,
    explicit: bool,
) -> Result<(), ColorSchemeParseError> {
    let value = match JsonMember::from_object(object, "colorScheme") {
        JsonMember::Missing | JsonMember::Null => return Ok(()),
        JsonMember::Value(value) => value,
    };

    match value {
        JsonValue::String(name) => {
            appearance.light.set(name, source, explicit);
            appearance.dark.set(name, source, explicit);
            Ok(())
        }
        JsonValue::Object(pair) => {
            apply_reference_side(pair, "light", &mut appearance.light, source, explicit)?;
            apply_reference_side(pair, "dark", &mut appearance.dark, source, explicit)?;
            Ok(())
        }
        _ => Err(ColorSchemeParseError::InvalidReference),
    }
}

fn apply_reference_side(
    object: &JsonObject,
    key: &str,
    slot: &mut SchemeSlot,
    source: ReferenceSource,
    explicit: bool,
) -> Result<(), ColorSchemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(()),
        JsonMember::Value(JsonValue::String(name)) => {
            slot.set(name, source, explicit);
            Ok(())
        }
        JsonMember::Value(_) => Err(ColorSchemeParseError::InvalidReference),
    }
}

fn retarget_explicit_slot(slot: &mut SchemeSlot, old_name: &str, new_name: &str) {
    if slot.explicit && slot.value == old_name {
        slot.set(new_name, ReferenceSource::Explicit, true);
    }
}

fn retarget_default_collision_slot(
    slot: &mut SchemeSlot,
    old_name: &str,
    new_name: &str,
    collided_origin: OriginTag,
) {
    if slot.value != old_name {
        return;
    }
    if slot.explicit
        || (slot.source == ReferenceSource::Fallback && collided_origin == OriginTag::InBox)
    {
        slot.set(new_name, ReferenceSource::Explicit, true);
    }
}

fn retarget_collision_slot(slot: &mut SchemeSlot, old_name: &str, new_name: &str) {
    if slot.value != old_name {
        return;
    }
    if matches!(
        slot.source,
        ReferenceSource::Explicit | ReferenceSource::FragmentParent
    ) {
        slot.set(new_name, ReferenceSource::Explicit, true);
    }
}

fn refresh_slot_from_parent(
    slot: &mut SchemeSlot,
    parent: &SchemeSlot,
    inherited_source: ReferenceSource,
) {
    if slot.source == inherited_source {
        slot.value.clone_from(&parent.value);
    }
}

fn parse_schemes(input: &str) -> Result<Vec<ColorScheme>, ColorSchemeParseError> {
    let document = parse_document(input)?;
    let root = document
        .as_object()
        .ok_or(ColorSchemeParseError::ExpectedObject)?;
    match JsonMember::from_object(root, "schemes") {
        JsonMember::Missing | JsonMember::Null => Ok(Vec::new()),
        JsonMember::Value(JsonValue::Array(values)) => values
            .iter()
            .map(|value| {
                ColorScheme::from_object(
                    value
                        .as_object()
                        .ok_or(ColorSchemeParseError::ExpectedObject)?,
                )
            })
            .collect(),
        JsonMember::Value(_) => Err(ColorSchemeParseError::ExpectedArray),
    }
}

fn parse_document(input: &str) -> Result<JsonValue, ColorSchemeParseError> {
    settings_json::parse(input).map_err(|_| ColorSchemeParseError::InvalidJson)
}

fn required_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<&'a str, ColorSchemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Err(ColorSchemeParseError::MissingMember),
        JsonMember::Value(JsonValue::String(value)) => Ok(value),
        JsonMember::Value(_) => Err(ColorSchemeParseError::InvalidString),
    }
}

fn optional_color(
    object: &JsonObject,
    key: &str,
    default: Color,
) -> Result<Color, ColorSchemeParseError> {
    match JsonMember::from_object(object, key) {
        JsonMember::Missing | JsonMember::Null => Ok(default),
        JsonMember::Value(JsonValue::String(value)) => parse_color(value),
        JsonMember::Value(_) => Err(ColorSchemeParseError::InvalidColor),
    }
}

fn required_color(object: &JsonObject, key: &str) -> Result<Color, ColorSchemeParseError> {
    parse_color(required_string(object, key)?)
}

fn parse_color(value: &str) -> Result<Color, ColorSchemeParseError> {
    let digits = value
        .strip_prefix('#')
        .ok_or(ColorSchemeParseError::InvalidColor)?;
    if digits.len() != 6 || !digits.is_ascii() {
        return Err(ColorSchemeParseError::InvalidColor);
    }
    Ok(Color::rgb(
        parse_hex_byte(&digits[0..2])?,
        parse_hex_byte(&digits[2..4])?,
        parse_hex_byte(&digits[4..6])?,
    ))
}

fn parse_hex_byte(value: &str) -> Result<u8, ColorSchemeParseError> {
    u8::from_str_radix(value, 16).map_err(|_| ColorSchemeParseError::InvalidColor)
}

fn format_color(color: Color) -> String {
    format!("#{:02X}{:02X}{:02X}", color.r, color.g, color.b)
}
