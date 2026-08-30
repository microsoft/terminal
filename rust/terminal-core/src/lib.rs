//! Safe, platform-neutral state for Windows Terminal `TerminalCore`.
//!
//! R05 migrates deterministic core state before any C++ compatibility facade
//! or WinRT/COM boundary is introduced.

#![forbid(unsafe_code)]
#![expect(
    clippy::too_many_arguments,
    reason = "the update-selection entry point mirrors the upstream compatibility call shape"
)]

pub mod control_key_states;
pub mod keyboard_selection;
#[allow(unused_assignments)]
pub mod legacy_host_selection;
pub mod selection;
pub mod selection_boundary;
pub mod selection_rendering;
pub mod terminal;
pub mod terminal_api;
#[allow(clippy::missing_errors_doc)]
pub mod terminal_buffer_state;
pub mod terminal_wrap_state;
pub mod update_selection;
