//! Shared deterministic JSON/JSONC core for portable `SettingsModel` owners.
//!
//! Windows Terminal settings need to distinguish an omitted property from an
//! explicit `null`. This module provides that distinction once so individual
//! settings owners do not grow ad-hoc string scanners.

use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq)]
pub enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<JsonValue>),
    Object(JsonObject),
}

pub type JsonObject = BTreeMap<String, JsonValue>;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum JsonMember<'a> {
    Missing,
    Null,
    Value(&'a JsonValue),
}

impl<'a> JsonMember<'a> {
    #[must_use]
    pub fn from_object(object: &'a JsonObject, key: &str) -> Self {
        match object.get(key) {
            None => Self::Missing,
            Some(JsonValue::Null) => Self::Null,
            Some(value) => Self::Value(value),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum LayerValue<T> {
    Inherit,
    Null,
    Value(T),
}

impl<T> LayerValue<T> {
    pub fn overlay(&mut self, next: Self) {
        if !matches!(next, Self::Inherit) {
            *self = next;
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum JsonErrorKind {
    UnexpectedEnd,
    UnexpectedToken,
    InvalidEscape,
    InvalidUnicodeEscape,
    InvalidNumber,
    TrailingData,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct JsonError {
    pub offset: usize,
    pub kind: JsonErrorKind,
}

/// Parses one Windows Terminal settings JSON/JSONC document.
///
/// # Errors
///
/// Returns [`JsonError`] when the document is malformed, contains an invalid
/// escape/number, ends unexpectedly, or has trailing non-trivia data.
pub fn parse(input: &str) -> Result<JsonValue, JsonError> {
    let mut parser = Parser::new(input);
    let value = parser.parse_value()?;
    parser.skip_trivia()?;
    if parser.offset != input.len() {
        return Err(parser.error(JsonErrorKind::TrailingData));
    }
    Ok(value)
}

impl JsonValue {
    #[must_use]
    pub fn as_object(&self) -> Option<&JsonObject> {
        match self {
            Self::Object(value) => Some(value),
            _ => None,
        }
    }

    #[must_use]
    pub fn as_array(&self) -> Option<&[JsonValue]> {
        match self {
            Self::Array(value) => Some(value),
            _ => None,
        }
    }

    #[must_use]
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::String(value) => Some(value),
            _ => None,
        }
    }

    #[must_use]
    pub const fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(value) => Some(*value),
            _ => None,
        }
    }

    #[must_use]
    pub const fn as_f64(&self) -> Option<f64> {
        match self {
            Self::Number(value) => Some(*value),
            _ => None,
        }
    }
}

struct Parser<'a> {
    input: &'a str,
    offset: usize,
}

impl<'a> Parser<'a> {
    const fn new(input: &'a str) -> Self {
        Self { input, offset: 0 }
    }

    const fn error(&self, kind: JsonErrorKind) -> JsonError {
        JsonError {
            offset: self.offset,
            kind,
        }
    }

    fn parse_value(&mut self) -> Result<JsonValue, JsonError> {
        self.skip_trivia()?;
        match self.peek() {
            Some(b'n') => {
                self.expect_bytes(b"null")?;
                Ok(JsonValue::Null)
            }
            Some(b't') => {
                self.expect_bytes(b"true")?;
                Ok(JsonValue::Bool(true))
            }
            Some(b'f') => {
                self.expect_bytes(b"false")?;
                Ok(JsonValue::Bool(false))
            }
            Some(b'"') => self.parse_string().map(JsonValue::String),
            Some(b'[') => self.parse_array().map(JsonValue::Array),
            Some(b'{') => self.parse_object().map(JsonValue::Object),
            Some(b'-' | b'0'..=b'9') => self.parse_number().map(JsonValue::Number),
            Some(_) => Err(self.error(JsonErrorKind::UnexpectedToken)),
            None => Err(self.error(JsonErrorKind::UnexpectedEnd)),
        }
    }

    fn parse_object(&mut self) -> Result<JsonObject, JsonError> {
        self.consume(b'{')?;
        let mut result = BTreeMap::new();
        loop {
            self.skip_trivia()?;
            if self.take_if(b'}') {
                return Ok(result);
            }
            let key = self.parse_string()?;
            self.skip_trivia()?;
            self.consume(b':')?;
            let value = self.parse_value()?;
            result.insert(key, value);
            self.skip_trivia()?;
            if self.take_if(b'}') {
                return Ok(result);
            }
            self.consume(b',')?;
            self.skip_trivia()?;
            // JsonCpp accepts the trailing comma used by Microsoft settings tests.
            if self.take_if(b'}') {
                return Ok(result);
            }
        }
    }

    fn parse_array(&mut self) -> Result<Vec<JsonValue>, JsonError> {
        self.consume(b'[')?;
        let mut result = Vec::new();
        loop {
            self.skip_trivia()?;
            if self.take_if(b']') {
                return Ok(result);
            }
            result.push(self.parse_value()?);
            self.skip_trivia()?;
            if self.take_if(b']') {
                return Ok(result);
            }
            self.consume(b',')?;
            self.skip_trivia()?;
            if self.take_if(b']') {
                return Ok(result);
            }
        }
    }

    fn parse_string(&mut self) -> Result<String, JsonError> {
        self.consume(b'"')?;
        let mut result = String::new();
        while let Some(byte) = self.peek() {
            match byte {
                b'"' => {
                    self.offset += 1;
                    return Ok(result);
                }
                b'\\' => {
                    self.offset += 1;
                    let escaped = self
                        .next()
                        .ok_or_else(|| self.error(JsonErrorKind::UnexpectedEnd))?;
                    match escaped {
                        b'"' => result.push('"'),
                        b'\\' => result.push('\\'),
                        b'/' => result.push('/'),
                        b'b' => result.push('\u{0008}'),
                        b'f' => result.push('\u{000c}'),
                        b'n' => result.push('\n'),
                        b'r' => result.push('\r'),
                        b't' => result.push('\t'),
                        b'u' => result.push(self.parse_unicode_escape()?),
                        _ => return Err(self.error(JsonErrorKind::InvalidEscape)),
                    }
                }
                0x00..=0x1f => return Err(self.error(JsonErrorKind::UnexpectedToken)),
                _ => {
                    let remainder = &self.input[self.offset..];
                    let ch = remainder
                        .chars()
                        .next()
                        .ok_or_else(|| self.error(JsonErrorKind::UnexpectedEnd))?;
                    result.push(ch);
                    self.offset += ch.len_utf8();
                }
            }
        }
        Err(self.error(JsonErrorKind::UnexpectedEnd))
    }

    fn parse_unicode_escape(&mut self) -> Result<char, JsonError> {
        let high = self.parse_hex_quad()?;
        if (0xd800..=0xdbff).contains(&high) {
            if !self.remaining().starts_with("\\u") {
                return Err(self.error(JsonErrorKind::InvalidUnicodeEscape));
            }
            self.offset += 2;
            let low = self.parse_hex_quad()?;
            if !(0xdc00..=0xdfff).contains(&low) {
                return Err(self.error(JsonErrorKind::InvalidUnicodeEscape));
            }
            let codepoint =
                0x10000 + (((u32::from(high) - 0xd800) << 10) | (u32::from(low) - 0xdc00));
            char::from_u32(codepoint).ok_or_else(|| self.error(JsonErrorKind::InvalidUnicodeEscape))
        } else if (0xdc00..=0xdfff).contains(&high) {
            Err(self.error(JsonErrorKind::InvalidUnicodeEscape))
        } else {
            char::from_u32(u32::from(high))
                .ok_or_else(|| self.error(JsonErrorKind::InvalidUnicodeEscape))
        }
    }

    fn parse_hex_quad(&mut self) -> Result<u16, JsonError> {
        if self.offset + 4 > self.input.len() {
            return Err(self.error(JsonErrorKind::UnexpectedEnd));
        }
        let digits = &self.input[self.offset..self.offset + 4];
        self.offset += 4;
        u16::from_str_radix(digits, 16).map_err(|_| self.error(JsonErrorKind::InvalidUnicodeEscape))
    }

    fn parse_number(&mut self) -> Result<f64, JsonError> {
        let start = self.offset;
        if self.take_if(b'-') && self.peek().is_none() {
            return Err(self.error(JsonErrorKind::InvalidNumber));
        }
        if self.take_if(b'0') {
            // leading zero is complete integer portion
        } else {
            self.take_digits();
        }
        if self.take_if(b'.') {
            let before = self.offset;
            self.take_digits();
            if self.offset == before {
                return Err(self.error(JsonErrorKind::InvalidNumber));
            }
        }
        if matches!(self.peek(), Some(b'e' | b'E')) {
            self.offset += 1;
            if matches!(self.peek(), Some(b'+' | b'-')) {
                self.offset += 1;
            }
            let before = self.offset;
            self.take_digits();
            if self.offset == before {
                return Err(self.error(JsonErrorKind::InvalidNumber));
            }
        }
        self.input[start..self.offset]
            .parse::<f64>()
            .map_err(|_| self.error(JsonErrorKind::InvalidNumber))
    }

    fn take_digits(&mut self) {
        while matches!(self.peek(), Some(b'0'..=b'9')) {
            self.offset += 1;
        }
    }

    fn skip_trivia(&mut self) -> Result<(), JsonError> {
        loop {
            while matches!(self.peek(), Some(b' ' | b'\t' | b'\r' | b'\n')) {
                self.offset += 1;
            }
            if self.remaining().starts_with("//") {
                self.offset += 2;
                while let Some(byte) = self.peek() {
                    self.offset += 1;
                    if byte == b'\n' {
                        break;
                    }
                }
                continue;
            }
            if self.remaining().starts_with("/*") {
                self.offset += 2;
                let Some(end) = self.remaining().find("*/") else {
                    return Err(self.error(JsonErrorKind::UnexpectedEnd));
                };
                self.offset += end + 2;
                continue;
            }
            return Ok(());
        }
    }

    fn expect_bytes(&mut self, expected: &[u8]) -> Result<(), JsonError> {
        if self
            .input
            .as_bytes()
            .get(self.offset..self.offset + expected.len())
            == Some(expected)
        {
            self.offset += expected.len();
            Ok(())
        } else {
            Err(self.error(JsonErrorKind::UnexpectedToken))
        }
    }

    fn consume(&mut self, expected: u8) -> Result<(), JsonError> {
        if self.take_if(expected) {
            Ok(())
        } else {
            Err(self.error(JsonErrorKind::UnexpectedToken))
        }
    }

    fn take_if(&mut self, expected: u8) -> bool {
        if self.peek() == Some(expected) {
            self.offset += 1;
            true
        } else {
            false
        }
    }

    fn peek(&self) -> Option<u8> {
        self.input.as_bytes().get(self.offset).copied()
    }

    fn next(&mut self) -> Option<u8> {
        let byte = self.peek()?;
        self.offset += 1;
        Some(byte)
    }

    fn remaining(&self) -> &str {
        &self.input[self.offset..]
    }
}
