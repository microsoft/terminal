//! Safe Rust migration track for Windows Terminal's adapter layer.
//!
//! R03 isolates protocol-heavy Adapter components from the C++ `TextBuffer`,
//! renderer, and platform surfaces. `PageManager` completes the deterministic
//! VT paging control plane while concrete page storage remains an R04 concern.

#![forbid(unsafe_code)]

pub mod adapt_dispatch;
pub mod checksum_reports;
#[allow(
    clippy::format_push_string,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::float_cmp
)]
pub mod color_product_dispatch;
pub mod dcs_dispatch;
pub mod decrqss;
pub mod decrqss_color_alias;
pub mod decrqss_cursor;
pub mod input_mode_dispatch;
pub mod line_feed_product;
pub mod macro_buffer;
pub mod macro_execution;
pub mod macro_reports;
pub mod page_manager;
pub mod page_storage;
pub mod parser_control;
pub mod presentation_reports;
pub mod presentation_state;
pub mod product_dispatch;
pub mod reporting_product_dispatch;
pub mod response_dispatch;
pub mod screen_buffer_cursor;
pub mod sixel;
#[allow(clippy::too_many_lines)]
pub mod soft_font_size;
pub mod terminal_surface_product;
pub mod user_preference_charset;
pub mod vt_response;
pub mod window_reports;
