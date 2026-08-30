//! Product wrapper for command-palette filtering.
//!
//! The fuzzy matcher owns score and match positions. `FilteredCommand` owns the
//! TerminalApp-facing projection of that result into weight, highlighting and
//! stable command ordering.

use crate::fzf::{Pattern, TextRun, match_text};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FilteredCommand {
    name: String,
    weight: i32,
    highlights: Option<Vec<TextRun>>,
}

impl FilteredCommand {
    #[must_use]
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            weight: 0,
            highlights: None,
        }
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub const fn weight(&self) -> i32 {
        self.weight
    }

    #[must_use]
    pub fn name_highlights(&self) -> Option<&[TextRun]> {
        self.highlights.as_deref()
    }

    pub fn update_filter(&mut self, pattern: Option<&Pattern>) {
        let Some(pattern) = pattern else {
            self.weight = 0;
            self.highlights = None;
            return;
        };

        if let Some(result) = match_text(&self.name, pattern) {
            self.weight = result.score;
            self.highlights = (!result.runs.is_empty()).then_some(result.runs);
        } else {
            self.weight = 0;
            self.highlights = None;
        }
    }

    #[must_use]
    pub fn compare(left: &Self, right: &Self) -> bool {
        if left.weight != right.weight {
            return left.weight > right.weight;
        }

        left.name
            .chars()
            .flat_map(char::to_lowercase)
            .cmp(right.name.chars().flat_map(char::to_lowercase))
            .is_lt()
    }
}
