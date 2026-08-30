//! Host-level search cursor/direction orchestration over terminal-buffer spans.
//!
//! Native `Search` delegates matching to `TextBuffer::SearchText` and owns only
//! result navigation plus the focused span. This safe Rust owner keeps the same
//! split: `terminal-buffer` performs matching and this module controls Reset /
//! `FindNext` semantics without taking a dependency on Win32 selection globals.

use std::ops::BitOr;

use terminal_buffer::search::{SearchTextOptions, TextSearchSpan};
use terminal_buffer::text_buffer::TextBuffer;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SearchFlags(u8);

impl SearchFlags {
    pub const NONE: Self = Self(0);
    pub const CASE_INSENSITIVE: Self = Self(1 << 0);
    pub const REGULAR_EXPRESSION: Self = Self(1 << 1);

    #[must_use]
    pub const fn contains(self, other: Self) -> bool {
        self.0 & other.0 == other.0
    }
}

impl BitOr for SearchFlags {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SearchSession {
    results: Vec<TextSearchSpan>,
    index: Option<usize>,
    ok: bool,
}

impl SearchSession {
    #[must_use]
    pub fn reset(buffer: &TextBuffer, needle: &str, flags: SearchFlags, reverse: bool) -> Self {
        let options = SearchTextOptions {
            case_insensitive: flags.contains(SearchFlags::CASE_INSENSITIVE),
            regular_expression: flags.contains(SearchFlags::REGULAR_EXPRESSION),
        };

        match buffer.search_text_with_options(needle, options) {
            Ok(results) => {
                let index = if results.is_empty() {
                    None
                } else if reverse {
                    Some(results.len() - 1)
                } else {
                    Some(0)
                };
                Self {
                    results,
                    index,
                    ok: true,
                }
            }
            Err(_) => Self {
                results: Vec::new(),
                index: None,
                ok: false,
            },
        }
    }

    #[must_use]
    pub fn is_ok(&self) -> bool {
        self.ok
    }

    #[must_use]
    pub fn current(&self) -> Option<TextSearchSpan> {
        self.index.map(|index| self.results[index])
    }

    #[must_use]
    pub fn current_match(&self) -> Option<usize> {
        self.index
    }

    #[must_use]
    pub fn results(&self) -> &[TextSearchSpan] {
        &self.results
    }

    pub fn find_next(&mut self, reverse: bool) {
        let Some(index) = self.index else {
            return;
        };
        let count = self.results.len();
        self.index = Some(if reverse {
            if index == 0 { count - 1 } else { index - 1 }
        } else {
            (index + 1) % count
        });
    }
}
