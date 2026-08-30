//! Platform-neutral environment-table semantics from `til::env`.
//!
//! Windows user-environment regeneration remains a native boundary because it depends on
//! shell32, the registry and process-token state. This module owns the deterministic table,
//! environment-block serialization and `%NAME%` expansion behavior.

use std::collections::BTreeMap;

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Environment {
    entries: BTreeMap<String, EnvironmentEntry>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct EnvironmentEntry {
    name: String,
    value: String,
}

impl Environment {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            entries: BTreeMap::new(),
        }
    }

    /// Inserts or replaces an environment variable using case-insensitive key identity.
    pub fn insert(&mut self, name: impl Into<String>, value: impl Into<String>) {
        let name = name.into();
        let value = value.into();
        self.entries
            .insert(canonical_key(&name), EnvironmentEntry { name, value });
    }

    /// Serializes the table as a Windows-compatible UTF-16 environment block.
    ///
    /// Each entry is NUL-terminated and the complete block has an additional trailing NUL.
    #[must_use]
    pub fn to_utf16_block(&self) -> Vec<u16> {
        let mut result = Vec::new();
        for entry in self.entries.values() {
            result.extend(entry.name.encode_utf16());
            result.push(u16::from(b'='));
            result.extend(entry.value.encode_utf16());
            result.push(0);
        }
        result.push(0);
        result
    }

    /// Expands complete `%NAME%` references using the current table.
    ///
    /// Unknown variables and unmatched opening percent signs are preserved verbatim, matching
    /// the portable state machine in Microsoft's `til::env::expand_environment_strings`.
    #[must_use]
    pub fn expand_environment_strings(&self, input: &str) -> String {
        let mut expanded = String::with_capacity(input.len());
        let mut in_name = false;
        let mut name = String::new();

        for character in input.chars() {
            if character == '%' {
                if in_name {
                    if let Some(entry) = self.entries.get(&canonical_key(&name)) {
                        expanded.push_str(&entry.value);
                    } else {
                        expanded.push('%');
                        expanded.push_str(&name);
                        expanded.push('%');
                    }
                    in_name = false;
                    name.clear();
                } else {
                    in_name = true;
                }
            } else if in_name {
                name.push(character);
            } else {
                expanded.push(character);
            }
        }

        if in_name {
            expanded.push('%');
            expanded.push_str(&name);
        }

        expanded
    }
}

fn canonical_key(name: &str) -> String {
    name.chars().flat_map(char::to_lowercase).collect()
}

#[cfg(test)]
mod tests {
    use super::Environment;

    #[test]
    fn microsoft_til_env_construct() {
        let environment = Environment::new();
        assert_eq!(vec![0], environment.to_utf16_block());
    }

    #[test]
    fn microsoft_til_env_to_string() {
        let mut environment = Environment::new();
        environment.insert("A", "Apple");
        environment.insert("B", "Banana");
        environment.insert("C", "Cassowary");

        let mut expected: Vec<u16> = "A=Apple\0B=Banana\0C=Cassowary\0".encode_utf16().collect();
        expected.push(0);
        assert_eq!(expected, environment.to_utf16_block());
    }

    #[test]
    fn microsoft_til_env_expand_environment_strings() {
        let mut environment = Environment::new();
        environment.insert("ENV", "Bar");
        assert_eq!(
            "FooBarBaz",
            environment.expand_environment_strings("Foo%ENV%Baz")
        );

        let empty = Environment::new();
        assert_eq!(
            "Foo%ENV%Baz",
            empty.expand_environment_strings("Foo%ENV%Baz")
        );
        assert_eq!("Foo%ENV", empty.expand_environment_strings("Foo%ENV"));
    }

    #[test]
    fn environment_lookup_is_case_insensitive() {
        let mut environment = Environment::new();
        environment.insert("Path", "value");
        assert_eq!("value", environment.expand_environment_strings("%PATH%"));
    }
}
