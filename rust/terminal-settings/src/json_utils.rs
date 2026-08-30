//! Portable conversion helpers matching Windows Terminal's `SettingsModel` `JsonUtils` contract.
//!
//! The native helper distinguishes a missing object member from an explicit JSON
//! `null`, supports caller-defined conversion traits, and preserves destination
//! storage when conversion fails. This module owns those deterministic semantics
//! on top of the shared [`crate::settings_json::JsonValue`] representation.

use std::collections::BTreeMap;

use crate::settings_json::{JsonObject, JsonValue};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeserializationErrorKind {
    TypeMismatch,
    InvalidValue,
    UnknownMapping,
    ConflictingFlags,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeserializationError {
    pub key: Option<String>,
    pub kind: DeserializationErrorKind,
}

impl DeserializationError {
    #[must_use]
    pub const fn new(kind: DeserializationErrorKind) -> Self {
        Self { key: None, kind }
    }

    #[must_use]
    pub fn with_key(mut self, key: &str) -> Self {
        if self.key.is_none() {
            self.key = Some(key.to_owned());
        }
        self
    }
}

pub trait JsonConversion: Sized {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError>;
    fn to_json(&self) -> JsonValue;

    /// Whether `null` is a concrete value for this type rather than the absence
    /// marker used by an enclosing `Option<T>`.
    #[must_use]
    fn accepts_null() -> bool {
        false
    }
}

pub trait JsonConverter<T> {
    fn from_json(&self, value: &JsonValue) -> Result<T, DeserializationError>;
}

/// Converts one JSON value, throwing a typed conversion error on mismatch.
pub fn get_value<T: JsonConversion>(value: &JsonValue) -> Result<T, DeserializationError> {
    T::from_json(value)
}

/// Converts into existing storage only after conversion has succeeded.
///
/// This preserves the caller's previous value when conversion fails, matching
/// Microsoft's filling overload.
pub fn fill_value<T: JsonConversion>(
    value: &JsonValue,
    output: &mut T,
) -> Result<bool, DeserializationError> {
    let converted = T::from_json(value)?;
    *output = converted;
    Ok(true)
}

/// Returns a converted object member, or `T::default()` when the key is absent.
pub fn get_value_for_key<T: JsonConversion + Default>(
    object: &JsonObject,
    key: &str,
) -> Result<T, DeserializationError> {
    match object.get(key) {
        Some(value) => T::from_json(value).map_err(|error| error.with_key(key)),
        None => Ok(T::default()),
    }
}

/// Fills an existing object member when present; a missing key is a no-op.
pub fn fill_value_for_key<T: JsonConversion>(
    object: &JsonObject,
    key: &str,
    output: &mut T,
) -> Result<bool, DeserializationError> {
    let Some(value) = object.get(key) else {
        return Ok(false);
    };
    let converted = T::from_json(value).map_err(|error| error.with_key(key))?;
    *output = converted;
    Ok(true)
}

pub fn get_value_with<T, C: JsonConverter<T>>(
    value: &JsonValue,
    converter: &C,
) -> Result<T, DeserializationError> {
    converter.from_json(value)
}

pub fn get_value_for_key_with<T: Default, C: JsonConverter<T>>(
    object: &JsonObject,
    key: &str,
    converter: &C,
) -> Result<T, DeserializationError> {
    match object.get(key) {
        Some(value) => converter
            .from_json(value)
            .map_err(|error| error.with_key(key)),
        None => Ok(T::default()),
    }
}

pub fn set_value_for_key<T: JsonConversion>(object: &mut JsonObject, key: &str, value: &T) {
    object.insert(key.to_owned(), value.to_json());
}

/// Serializes an optional object property. `None` means the member is omitted;
/// `Some(value)` delegates to the inner conversion, which may itself emit null.
pub fn set_optional_for_key<T: JsonConversion>(
    object: &mut JsonObject,
    key: &str,
    value: &Option<T>,
) {
    match value {
        Some(value) => {
            object.insert(key.to_owned(), value.to_json());
        }
        None => {
            object.remove(key);
        }
    }
}

impl<T: JsonConversion> JsonConversion for Option<T> {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        if matches!(value, JsonValue::Null) {
            if T::accepts_null() {
                T::from_json(value).map(Some)
            } else {
                Ok(None)
            }
        } else {
            T::from_json(value).map(Some)
        }
    }

    fn to_json(&self) -> JsonValue {
        match self {
            Some(value) => value.to_json(),
            None => JsonValue::Null,
        }
    }

    fn accepts_null() -> bool {
        true
    }
}

impl JsonConversion for String {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        value
            .as_str()
            .map(ToOwned::to_owned)
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::String(self.clone())
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct WideString(pub String);

impl JsonConversion for WideString {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        String::from_json(value).map(Self)
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::String(self.0.clone())
    }
}

fn number(value: &JsonValue) -> Result<f64, DeserializationError> {
    value
        .as_f64()
        .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))
}

impl JsonConversion for i32 {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        let value = number(value)?;
        if value.is_finite()
            && value.fract() == 0.0
            && value >= f64::from(i32::MIN)
            && value <= f64::from(i32::MAX)
        {
            Ok(value as Self)
        } else {
            Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ))
        }
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::Number(f64::from(*self))
    }
}

impl JsonConversion for u32 {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        let value = number(value)?;
        if value.is_finite() && value.fract() == 0.0 && value >= 0.0 && value <= f64::from(u32::MAX)
        {
            Ok(value as Self)
        } else {
            Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ))
        }
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::Number(f64::from(*self))
    }
}

impl JsonConversion for bool {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        value
            .as_bool()
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::Bool(*self)
    }
}

impl JsonConversion for f32 {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        Ok(number(value)? as Self)
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::Number(f64::from(*self))
    }
}

impl JsonConversion for f64 {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        number(value)
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::Number(*self)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RgbColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl JsonConversion for RgbColor {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        let text = value
            .as_str()
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))?;
        let Some(hex) = text.strip_prefix('#') else {
            return Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ));
        };
        let expanded;
        let hex = if hex.len() == 3 {
            expanded = hex.chars().flat_map(|ch| [ch, ch]).collect::<String>();
            expanded.as_str()
        } else if hex.len() == 6 {
            hex
        } else {
            return Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ));
        };
        let parse = |range: std::ops::Range<usize>| {
            u8::from_str_radix(&hex[range], 16)
                .map_err(|_| DeserializationError::new(DeserializationErrorKind::InvalidValue))
        };
        Ok(Self {
            r: parse(0..2)?,
            g: parse(2..4)?,
            b: parse(4..6)?,
        })
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::String(format!("#{:02X}{:02X}{:02X}", self.r, self.g, self.b))
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Guid(String);

impl Guid {
    pub fn parse(input: &str) -> Result<Self, DeserializationError> {
        if input.len() != 38
            || !input.starts_with('{')
            || !input.ends_with('}')
            || !matches!(input.as_bytes().get(9), Some(b'-'))
            || !matches!(input.as_bytes().get(14), Some(b'-'))
            || !matches!(input.as_bytes().get(19), Some(b'-'))
            || !matches!(input.as_bytes().get(24), Some(b'-'))
        {
            return Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ));
        }
        if input[1..37]
            .chars()
            .enumerate()
            .any(|(index, ch)| !matches!(index, 8 | 13 | 18 | 23) && !ch.is_ascii_hexdigit())
        {
            return Err(DeserializationError::new(
                DeserializationErrorKind::InvalidValue,
            ));
        }
        Ok(Self(input.to_ascii_lowercase()))
    }

    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl JsonConversion for Guid {
    fn from_json(value: &JsonValue) -> Result<Self, DeserializationError> {
        let text = value
            .as_str()
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))?;
        Self::parse(text)
    }

    fn to_json(&self) -> JsonValue {
        JsonValue::String(self.0.clone())
    }
}

#[derive(Debug, Clone, Copy)]
pub struct EnumMapper<T: Copy + Eq + 'static> {
    mappings: &'static [(&'static str, T)],
}

impl<T: Copy + Eq + 'static> EnumMapper<T> {
    #[must_use]
    pub const fn new(mappings: &'static [(&'static str, T)]) -> Self {
        Self { mappings }
    }

    pub fn from_json(&self, value: &JsonValue) -> Result<T, DeserializationError> {
        let text = value
            .as_str()
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::TypeMismatch))?;
        self.mappings
            .iter()
            .find_map(|(name, value)| (*name == text).then_some(*value))
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::UnknownMapping))
    }

    pub fn to_json(&self, value: T) -> Result<JsonValue, DeserializationError> {
        self.mappings
            .iter()
            .find_map(|(name, candidate)| {
                (*candidate == value).then(|| JsonValue::String((*name).to_owned()))
            })
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::UnknownMapping))
    }
}

#[derive(Debug, Clone, Copy)]
pub struct FlagMapper {
    mappings: &'static [(&'static str, u32)],
    none_name: &'static str,
    all_name: &'static str,
    all_bits: u32,
}

impl FlagMapper {
    #[must_use]
    pub const fn new(
        mappings: &'static [(&'static str, u32)],
        none_name: &'static str,
        all_name: &'static str,
        all_bits: u32,
    ) -> Self {
        Self {
            mappings,
            none_name,
            all_name,
            all_bits,
        }
    }

    pub fn from_json(&self, value: &JsonValue) -> Result<u32, DeserializationError> {
        match value {
            JsonValue::String(value) => self.parse_one(value),
            JsonValue::Array(values) => {
                if values.is_empty() {
                    return Ok(0);
                }
                let mut result = 0_u32;
                let mut saw_none = false;
                for value in values {
                    let text = value.as_str().ok_or_else(|| {
                        DeserializationError::new(DeserializationErrorKind::TypeMismatch)
                    })?;
                    if text == self.none_name {
                        saw_none = true;
                        continue;
                    }
                    if text == self.all_name {
                        result = self.all_bits;
                        continue;
                    }
                    result |= self.parse_regular(text)?;
                }
                if saw_none && values.len() > 1 {
                    return Err(DeserializationError::new(
                        DeserializationErrorKind::ConflictingFlags,
                    ));
                }
                Ok(if saw_none { 0 } else { result })
            }
            _ => Err(DeserializationError::new(
                DeserializationErrorKind::TypeMismatch,
            )),
        }
    }

    pub fn to_json(&self, value: u32) -> Result<JsonValue, DeserializationError> {
        if value == 0 {
            return Ok(JsonValue::String(self.none_name.to_owned()));
        }
        if value == self.all_bits {
            return Ok(JsonValue::String(self.all_name.to_owned()));
        }
        if let Some((name, _)) = self.mappings.iter().find(|(_, bits)| *bits == value) {
            return Ok(JsonValue::String((*name).to_owned()));
        }
        let mut remaining = value;
        let mut result = Vec::new();
        for (name, bits) in self.mappings {
            if *bits != 0 && remaining & *bits == *bits {
                result.push(JsonValue::String((*name).to_owned()));
                remaining &= !*bits;
            }
        }
        if remaining != 0 || result.is_empty() {
            return Err(DeserializationError::new(
                DeserializationErrorKind::UnknownMapping,
            ));
        }
        Ok(JsonValue::Array(result))
    }

    fn parse_one(&self, value: &str) -> Result<u32, DeserializationError> {
        if value == self.none_name {
            Ok(0)
        } else if value == self.all_name {
            Ok(self.all_bits)
        } else {
            self.parse_regular(value)
        }
    }

    fn parse_regular(&self, value: &str) -> Result<u32, DeserializationError> {
        self.mappings
            .iter()
            .find_map(|(name, bits)| (*name == value).then_some(*bits))
            .ok_or_else(|| DeserializationError::new(DeserializationErrorKind::UnknownMapping))
    }
}

/// Convenience builder used by Microsoft-derived witnesses and product callers.
#[must_use]
pub fn object(entries: impl IntoIterator<Item = (impl Into<String>, JsonValue)>) -> JsonObject {
    entries
        .into_iter()
        .map(|(key, value)| (key.into(), value))
        .collect::<BTreeMap<_, _>>()
}
