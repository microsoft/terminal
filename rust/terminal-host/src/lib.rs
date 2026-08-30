//! Safe host/server/ConPTY foundations for the Windows Terminal Rust migration.

#![forbid(unsafe_code)]

pub mod alias;
pub mod api_detection;
pub mod api_message_buffers;
#[allow(
    clippy::struct_excessive_bools,
    clippy::needless_pass_by_value,
    clippy::missing_errors_doc
)]
pub mod api_routines;
pub mod api_sorter;
pub mod attribute_format;
pub mod codepage;
#[allow(clippy::missing_panics_doc, clippy::assigning_clones)]
pub mod command_history;
pub mod console_argument_parser;
pub mod console_arguments;
#[allow(clippy::missing_errors_doc)]
pub mod console_output_mode;
pub mod console_shim_policy;
pub mod event_synthesis;
pub mod host_signal;
pub mod input_buffer;
pub mod interactivity_factory;
pub mod keyboard_modifier_plan;
pub mod pty_clear_buffer;
pub mod pty_signal;
pub mod pty_signal_session;
pub mod pty_signal_state;
pub mod pty_signal_stream;
pub mod raw_console_arguments;
pub mod remote_console_control;
pub mod search;
pub mod title_translation;
pub mod vt_api_redirection;
pub mod vt_char_info;
pub mod vt_console_output;
pub mod vt_handles;
pub mod vt_io_protocol;
pub mod vt_io_state;
#[allow(clippy::missing_panics_doc)]
pub mod vt_legacy_console_write;
pub mod vt_screen_dump;
#[allow(clippy::too_many_arguments, clippy::cast_sign_loss)]
pub mod vt_screen_scroll;
pub mod vt_writer_sequences;
