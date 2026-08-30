//! Portable workspace-persistence semantics from `SettingsModel` `ApplicationState`.
//!
//! File scheduling and elevation-specific path selection remain boundary work;
//! this owner captures the deterministic map mutations consumed by those I/O
//! seams and by startup workspace restoration.

use std::collections::BTreeMap;

/// Portable subset of the persisted window layout needed by the workspace map.
/// Additional layout fields are added as their `SettingsModel` contracts migrate.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct WindowLayout {
    tab_layout: Vec<String>,
}

impl WindowLayout {
    #[must_use]
    pub fn with_tab_layout(tab_layout: Vec<String>) -> Self {
        Self { tab_layout }
    }

    #[must_use]
    pub fn tab_layout(&self) -> &[String] {
        &self.tab_layout
    }
}

/// Canonical safe Rust owner for persisted workspace entries.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ApplicationState {
    persisted_workspaces: BTreeMap<String, WindowLayout>,
}

impl ApplicationState {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Inserts or replaces a persisted workspace under `name`.
    pub fn save_workspace(&mut self, name: impl Into<String>, layout: WindowLayout) {
        self.persisted_workspaces.insert(name.into(), layout);
    }

    /// Removes `name`, returning whether the map was modified.
    pub fn remove_workspace(&mut self, name: &str) -> bool {
        self.persisted_workspaces.remove(name).is_some()
    }

    /// Renames an entry with Microsoft's exact no-op/removal rules.
    ///
    /// Equal names and an empty old name are no-ops. An empty new name removes
    /// the old entry. A non-empty new name replaces any existing target entry.
    pub fn rename_workspace(&mut self, old_name: &str, new_name: &str) -> bool {
        if old_name == new_name || old_name.is_empty() {
            return false;
        }

        let Some(layout) = self.persisted_workspaces.remove(old_name) else {
            return false;
        };

        if !new_name.is_empty() {
            self.persisted_workspaces
                .insert(new_name.to_owned(), layout);
        }
        true
    }

    /// Atomically removes and returns the requested workspace.
    pub fn take_workspace(&mut self, name: &str) -> Option<WindowLayout> {
        self.persisted_workspaces.remove(name)
    }

    #[must_use]
    pub const fn all_persisted_workspaces(&self) -> &BTreeMap<String, WindowLayout> {
        &self.persisted_workspaces
    }
}
