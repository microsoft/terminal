//! Safe Rust owners for deterministic Windows Terminal settings semantics.
//!
//! The crate deliberately excludes XAML/WinRT projection. R08 moves portable
//! `SettingsModel` behavior here while the existing managed/native UI surfaces
//! remain responsible for presentation and ABI boundaries.

#![forbid(unsafe_code)]

#[allow(clippy::unnecessary_get_then_check, clippy::cast_possible_truncation)]
pub mod action_map;
pub mod application_state;
#[allow(clippy::missing_panics_doc)]
pub mod cascadia_settings;
pub mod color_scheme;
pub mod command_expansion;
pub mod command_model;
#[allow(clippy::unnecessary_wraps)]
pub mod deserialization_actions;
pub mod deserialization_copy;
#[allow(clippy::unnecessary_wraps)]
pub mod deserialization_fragments;
#[allow(clippy::cast_possible_truncation, clippy::match_same_arms)]
pub mod deserialization_profile_properties;
#[allow(clippy::missing_panics_doc, clippy::cast_possible_truncation)]
pub mod deserialization_profiles;
pub mod deserialization_validation;
pub mod elevate;
#[allow(
    clippy::missing_errors_doc,
    clippy::wrong_self_convention,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss
)]
pub mod json_utils;
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
pub mod keybindings;
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
pub mod keybindings_model;
#[allow(clippy::missing_panics_doc, clippy::assigning_clones)]
pub mod media_resource;
pub mod new_tab_menu;
pub mod profile;
pub mod profile_collection;
pub mod profile_duplication;
pub mod profile_identity;
pub mod profile_lookup;
#[allow(clippy::manual_let_else)]
pub mod serialization;
pub mod settings_fixup;
pub mod settings_json;
#[allow(
    clippy::unnecessary_sort_by,
    clippy::assigning_clones,
    clippy::cast_possible_truncation
)]
pub mod terminal_settings;
pub mod theme;
