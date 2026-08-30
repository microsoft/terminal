#![forbid(unsafe_code)]

mod env;
#[path = "lib.rs"]
mod foundation;
mod throttled;
mod til_math;
#[allow(clippy::too_many_lines)]
mod types_utils;
mod uuid;

pub use env::Environment;
pub use foundation::*;
pub use throttled::{Throttled, ThrottledError, ThrottledOptions};
pub use til_math::{IntegralRound, MathNarrowingError, checked_round_i32};
pub use types_utils::{
    WslStartingDirectoryResult, XTermColor, clamp_to_short_max, color_from_xterm_color,
    evaluate_starting_directory, filter_string_for_paste, mangle_starting_directory_for_wsl,
    split_string, string_to_uint, trim_paste,
};
pub use uuid::{Guid, create_v5_uuid};
