//! Portable DOSKEY alias expansion semantics used by the console host.
//!
//! The store owns case-insensitive EXE/alias lookup and deterministic `$` macro
//! expansion. Console locking and A/W code-page conversion stay at the native
//! API boundary.

use std::collections::BTreeMap;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct AliasStore {
    by_exe: BTreeMap<String, BTreeMap<String, String>>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AliasMatch {
    pub text: String,
    pub line_count: usize,
}

impl AliasStore {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add(&mut self, exe: &str, alias: &str, target: &str) {
        self.by_exe
            .entry(fold(exe))
            .or_default()
            .insert(fold(alias), target.to_owned());
    }

    /// Expands the first source token when it names an alias for `exe`.
    ///
    /// Leading spaces deliberately bypass alias processing. `$1`..`$9`, `$*`,
    /// `$l`, `$g`, `$b` and `$t` follow the native DOSKEY contract. Unknown
    /// escapes (including `$$`) are copied through unchanged. Successful
    /// expansion always appends CRLF and reports the number of produced lines.
    #[must_use]
    pub fn match_and_copy(&self, source: &str, exe: &str) -> Option<AliasMatch> {
        let aliases = self.by_exe.get(&fold(exe))?;
        if aliases.is_empty() || source.starts_with(' ') {
            return None;
        }

        let args = split_arguments(source);
        let alias = args.first()?;
        let target = aliases.get(&fold(alias))?;
        if target.is_empty() {
            return None;
        }

        let all_args = args
            .get(1)
            .and_then(|first| source.find(first))
            .map_or("", |offset| &source[offset..]);
        let mut output = String::new();
        let mut lines = 0;
        let chars = target.chars().collect::<Vec<_>>();
        let mut index = 0;
        while index < chars.len() {
            let ch = chars[index];
            index += 1;
            if ch != '$' || index == chars.len() {
                output.push(ch);
                continue;
            }

            let escaped = chars[index];
            index += 1;
            match escaped.to_ascii_lowercase() {
                '1'..='9' => {
                    let argument = escaped.to_digit(10).unwrap_or_default() as usize;
                    if let Some(value) = args.get(argument) {
                        output.push_str(value);
                    }
                }
                '*' => output.push_str(all_args),
                'l' => output.push('<'),
                'g' => output.push('>'),
                'b' => output.push('|'),
                't' => {
                    output.push_str("\r\n");
                    lines += 1;
                }
                _ => {
                    output.push('$');
                    output.push(escaped);
                }
            }
        }
        output.push_str("\r\n");
        lines += 1;
        Some(AliasMatch {
            text: output,
            line_count: lines,
        })
    }
}

fn fold(value: &str) -> String {
    value.chars().flat_map(char::to_lowercase).collect()
}

fn split_arguments(source: &str) -> Vec<&str> {
    let mut args = Vec::new();
    let bytes = source.as_bytes();
    let mut index = 0;
    while index < bytes.len() && args.len() < 10 {
        if bytes[index] == b' ' {
            break;
        }
        let start = index;
        while index < bytes.len() && bytes[index] != b' ' {
            index += 1;
        }
        args.push(&source[start..index]);
        while index < bytes.len() && bytes[index] == b' ' {
            index += 1;
        }
    }
    args
}
