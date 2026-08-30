//! Safe, platform-neutral foundations for Windows Terminal text buffers.
//!
//! R04 ports the deterministic storage and geometry semantics beneath the C++
//! `TextBuffer` before any C++ facade or FFI integration is introduced.

#![forbid(unsafe_code)]

#[allow(clippy::missing_panics_doc)]
pub mod alternate_buffer;
#[allow(clippy::manual_repeat_n)]
pub mod clipboard_text;
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
pub mod color_table;
#[allow(clippy::missing_panics_doc)]
pub mod command_regions;
#[allow(clippy::missing_panics_doc)]
pub mod cursor_movement;
pub mod cursor_properties;
#[allow(clippy::missing_panics_doc)]
pub mod deferred_resize;
#[allow(clippy::match_same_arms, clippy::missing_panics_doc)]
pub mod delayed_wrap;
#[allow(clippy::cast_possible_truncation)]
pub mod extended_attributes;
pub mod geometry;
#[allow(
    clippy::match_same_arms,
    clippy::missing_panics_doc,
    clippy::struct_excessive_bools,
    clippy::too_many_lines
)]
pub mod host_textbuffer;
#[allow(clippy::missing_errors_doc)]
pub mod host_write;
#[allow(clippy::struct_field_names)]
pub mod hyperlink;
pub mod image_slice;
#[allow(clippy::missing_errors_doc, clippy::missing_panics_doc)]
pub mod line_edit;
pub mod line_rendition;
pub mod output_cell;
pub mod output_cell_runs;
#[allow(clippy::missing_errors_doc)]
pub mod rect_ops;
pub mod reflow;
#[allow(clippy::unnecessary_wraps)]
pub mod reflow_cursor;
#[allow(clippy::missing_errors_doc)]
pub mod repeat_character;
pub mod resize_integrity;
pub mod rle;
#[allow(clippy::iter_cloned_collect, clippy::needless_pass_by_value)]
pub mod rle_ops;
pub mod row;
pub mod row_writer;
#[allow(clippy::items_after_statements, clippy::missing_panics_doc)]
pub mod rtf_text;
#[allow(clippy::missing_panics_doc)]
pub mod saved_cursor;
#[allow(clippy::missing_panics_doc, clippy::struct_excessive_bools)]
pub mod screen_alignment;
#[allow(clippy::missing_errors_doc)]
pub mod screen_erase;
#[allow(clippy::missing_errors_doc)]
pub mod search;
#[allow(clippy::cast_possible_truncation)]
pub mod sgr;
pub mod sixel_store;
#[allow(clippy::missing_panics_doc)]
pub mod soft_reset;
pub mod tab_stops;
#[allow(clippy::missing_errors_doc, clippy::struct_excessive_bools)]
pub mod terminal_modes;
pub mod text_attribute;
pub mod text_buffer;
#[allow(clippy::missing_errors_doc, clippy::missing_panics_doc)]
pub mod text_buffer_iterator;
pub mod text_buffer_queries;
#[allow(clippy::missing_errors_doc)]
pub mod text_buffer_write;
pub mod text_color;
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
pub mod til_color;
pub mod til_operators;
#[allow(
    clippy::cast_possible_truncation,
    clippy::cast_precision_loss,
    clippy::missing_errors_doc
)]
pub mod til_point;
#[allow(
    clippy::cast_possible_truncation,
    clippy::cast_precision_loss,
    clippy::manual_range_contains,
    clippy::missing_errors_doc
)]
pub mod til_rect;
#[allow(clippy::missing_errors_doc)]
pub mod til_rect_index;
pub mod til_replace;
#[allow(
    clippy::cast_possible_truncation,
    clippy::missing_panics_doc,
    clippy::unreadable_literal
)]
pub mod til_string;
#[allow(clippy::missing_panics_doc)]
pub mod til_utf_convert;
pub mod uia_text_range;
#[allow(clippy::missing_panics_doc)]
pub mod url_patterns;
#[allow(
    clippy::byte_char_slices,
    clippy::cast_possible_truncation,
    clippy::missing_errors_doc,
    clippy::missing_panics_doc
)]
pub mod vertical_scroll;
pub mod viewport;
#[allow(clippy::missing_errors_doc)]
pub mod viewport_index;
#[allow(clippy::filter_map_bool_then, clippy::missing_panics_doc)]
pub mod virtual_bottom;
#[allow(clippy::missing_panics_doc)]
pub mod vt_resize;
#[allow(clippy::cast_possible_truncation)]
pub mod width_detector;
pub mod word_boundary;

#[cfg(test)]
#[allow(clippy::match_same_arms)]
mod microsoft_reflow_tests;
#[cfg(test)]
#[allow(clippy::doc_markdown)]
mod microsoft_text_buffer_tests;
