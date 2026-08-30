//! Portable command-history storage and lifecycle semantics from conhost.
//!
//! Microsoft `CommandHistory` keeps a bounded MRU/LRU list of per-application
//! history buffers. Freeing a client detaches the process handle but deliberately
//! preserves the application's commands for a later reattachment in the same
//! console session. This owner carries those deterministic semantics without
//! depending on Win32 HANDLEs or global console state.

use std::collections::VecDeque;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandHistory {
    app_name: String,
    process_handle: Option<u64>,
    commands: Vec<String>,
    max_commands: usize,
}

impl CommandHistory {
    fn new(app_name: &str, process_handle: u64, max_commands: usize) -> Self {
        Self {
            app_name: app_name.to_owned(),
            process_handle: Some(process_handle),
            commands: Vec::new(),
            max_commands,
        }
    }

    #[must_use]
    pub fn app_name(&self) -> &str {
        &self.app_name
    }

    #[must_use]
    pub const fn process_handle(&self) -> Option<u64> {
        self.process_handle
    }

    #[must_use]
    pub const fn is_allocated(&self) -> bool {
        self.process_handle.is_some()
    }

    #[must_use]
    pub fn is_app_name_match(&self, other: &str) -> bool {
        names_equal_case_insensitive(&self.app_name, other)
    }

    #[must_use]
    pub fn command_count(&self) -> usize {
        self.commands.len()
    }

    #[must_use]
    pub fn commands(&self) -> &[String] {
        &self.commands
    }

    #[must_use]
    pub fn get_nth(&self, index: usize) -> Option<&str> {
        self.commands.get(index).map(String::as_str)
    }

    /// Adds one command using the native history buffer rules.
    ///
    /// Sequential duplicates are always suppressed. When `suppress_duplicates`
    /// is true, an earlier exact duplicate is removed and promoted to the newest
    /// slot. A full buffer evicts its oldest command.
    pub fn add(&mut self, command: &str, suppress_duplicates: bool) -> bool {
        if self.max_commands == 0 || !self.is_allocated() {
            return false;
        }
        if command.is_empty() {
            return true;
        }
        if self.commands.last().is_some_and(|last| last == command) {
            return true;
        }

        let mut value = command.to_owned();
        if suppress_duplicates
            && let Some(index) = self.commands.iter().rposition(|stored| stored == command)
        {
            value = self.commands.remove(index);
        }

        if self.commands.len() == self.max_commands {
            self.commands.remove(0);
        }
        self.commands.push(value);
        true
    }

    /// Resizes the command capacity, preserving Microsoft's oldest-first
    /// truncation behavior when shrinking.
    pub fn realloc(&mut self, max_commands: usize) {
        if self.max_commands == max_commands {
            return;
        }
        self.commands.truncate(max_commands);
        self.max_commands = max_commands;
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandHistoryStore {
    max_histories: usize,
    history_buffer_size: usize,
    histories: VecDeque<CommandHistory>,
}

impl CommandHistoryStore {
    #[must_use]
    pub fn new(max_histories: usize, history_buffer_size: usize) -> Self {
        Self {
            max_histories,
            history_buffer_size,
            histories: VecDeque::new(),
        }
    }

    #[must_use]
    pub fn history_count(&self) -> usize {
        self.histories.len()
    }

    #[must_use]
    pub fn find_by_handle(&self, process_handle: u64) -> Option<&CommandHistory> {
        self.histories
            .iter()
            .find(|history| history.process_handle == Some(process_handle))
    }

    #[must_use]
    pub fn find_by_handle_mut(&mut self, process_handle: u64) -> Option<&mut CommandHistory> {
        self.histories
            .iter_mut()
            .find(|history| history.process_handle == Some(process_handle))
    }

    /// Matches the native `s_FindByExe`: only allocated histories participate.
    #[must_use]
    pub fn find_by_exe(&self, app_name: &str) -> Option<&CommandHistory> {
        self.histories
            .iter()
            .find(|history| history.is_allocated() && history.is_app_name_match(app_name))
    }

    /// Exposes retained session storage, including a detached history waiting
    /// for the same application to reattach.
    #[must_use]
    pub fn find_stored_by_exe(&self, app_name: &str) -> Option<&CommandHistory> {
        self.histories
            .iter()
            .find(|history| history.is_app_name_match(app_name))
    }

    /// Allocates or reattaches one application history.
    ///
    /// Histories are ordered MRU at the front and LRU at the back. Reattaching
    /// the same application preserves its commands. A different application may
    /// reuse only a detached history; if every configured history is currently
    /// allocated, allocation fails exactly as in conhost.
    pub fn allocate(&mut self, app_name: &str, process_handle: u64) -> bool {
        if let Some(index) = self
            .histories
            .iter()
            .position(|history| !history.is_allocated() && history.is_app_name_match(app_name))
        {
            let mut history = self
                .histories
                .remove(index)
                .expect("candidate index came from the current deque");
            history.process_handle = Some(process_handle);
            self.histories.push_front(history);
            return true;
        }

        if self.histories.len() < self.max_histories {
            self.histories.push_front(CommandHistory::new(
                app_name,
                process_handle,
                self.history_buffer_size,
            ));
            return true;
        }

        let mut best_candidate = None;
        for (index, history) in self.histories.iter().enumerate() {
            if history.is_allocated() {
                continue;
            }
            let replace = history.commands.is_empty()
                || best_candidate.is_none()
                || best_candidate
                    .is_some_and(|best: usize| !self.histories[best].commands.is_empty());
            if replace {
                best_candidate = Some(index);
            }
        }

        let Some(index) = best_candidate else {
            return false;
        };
        let mut history = self
            .histories
            .remove(index)
            .expect("candidate index came from the current deque");
        history.commands.clear();
        history.app_name = app_name.to_owned();
        history.process_handle = Some(process_handle);
        self.histories.push_front(history);
        true
    }

    /// Detaches a process while retaining the application history for session
    /// reuse.
    pub fn free(&mut self, process_handle: u64) {
        if let Some(history) = self.find_by_handle_mut(process_handle) {
            history.process_handle = None;
        }
    }
}

fn names_equal_case_insensitive(left: &str, right: &str) -> bool {
    left.chars()
        .flat_map(char::to_lowercase)
        .eq(right.chars().flat_map(char::to_lowercase))
}
