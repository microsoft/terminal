#![forbid(unsafe_code)]

mod attribute_color_policy;
mod css_length_percentage;
mod font_info_base_policy;
mod font_info_desired_policy;
mod font_info_policy;
mod redraw_region;
mod render_settings_policy;
mod rendition_blink;
mod retry_policy;
mod timer_policy;
mod title_state;
mod viewport_update;

pub use attribute_color_policy::{
    AttributeColorFlags, AttributeColors, ResolvedAttributeColors, apply_attribute_alpha,
    apply_attribute_effects, resolve_text_attribute_colors,
};
pub use css_length_percentage::{CssLengthPercentage, ReferenceFrame};
pub use font_info_base_policy::{
    LEGACY_FACE_NAME_CAPACITY, is_default_raster_without_size, is_true_type_family,
    legacy_face_name_buffer,
};
pub use font_info_desired_policy::{CellSize, FontInfoDesiredPolicy};
pub use font_info_policy::{FontCellSizes, validate_font_cell_sizes};
pub use redraw_region::{ExclusiveRect, plan_redraw_region};
pub use render_settings_policy::{RenderMode, RenderSettingsPolicy};
pub use rendition_blink::{
    RENDITION_BLINK_INTERVAL_100NS, RenditionBlinkAction, plan_rendition_blink,
};
pub use retry_policy::{
    MAX_RETRIES_FOR_RENDER_ENGINE, RENDER_BACKOFF_BASE_MILLIS, RenderAttempt, render_attempts,
};
pub use timer_policy::{
    TIMER_REPR_MAX, TimerRepr, reschedule_repeating_timer, saturating_timer_add,
    saturating_timer_sub, timer_to_millis,
};
pub use title_state::{TitleState, TitleUpdate};
pub use viewport_update::{InclusiveRect, ScrollDelta, ViewportUpdate, plan_viewport_update};
