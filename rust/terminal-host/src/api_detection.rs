//! Pure policy extracted from interactivity `ApiDetector`.
//!
//! Library loading, procedure lookup, and `FreeLibrary` remain Win32-owned boundaries.

/// Classification of the first `LoadLibraryExW` attempt.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LoaderFailure {
    InvalidParameter,
    Other,
}

/// Interactivity implementation level selected after probing the API set.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ApiLevel {
    Win32,
    OneCore,
}

/// The no-forwarder loader flag is retried with `LOAD_LIBRARY_SEARCH_SYSTEM32` only when
/// the first call failed because that flag is unsupported on downlevel Windows.
#[must_use]
pub const fn retry_without_no_forwarder(
    first_load_succeeded: bool,
    failure: LoaderFailure,
) -> bool {
    !first_load_succeeded && matches!(failure, LoaderFailure::InvalidParameter)
}

/// Map the final library/procedure probe result to the implementation level used by C++.
#[must_use]
pub const fn select_api_level(
    library_loaded: bool,
    procedure_required: bool,
    procedure_found: bool,
) -> ApiLevel {
    if library_loaded && (!procedure_required || procedure_found) {
        ApiLevel::Win32
    } else {
        ApiLevel::OneCore
    }
}

/// C++ frees the loaded module whenever the final probe is unsuccessful.
#[must_use]
pub const fn should_free_module(level: ApiLevel, module_loaded: bool) -> bool {
    module_loaded && matches!(level, ApiLevel::OneCore)
}

#[cfg(test)]
mod tests {
    use super::{
        ApiLevel, LoaderFailure, retry_without_no_forwarder, select_api_level, should_free_module,
    };

    #[test]
    fn retries_only_for_unsupported_loader_flag() {
        assert!(retry_without_no_forwarder(
            false,
            LoaderFailure::InvalidParameter
        ));
        assert!(!retry_without_no_forwarder(false, LoaderFailure::Other));
        assert!(!retry_without_no_forwarder(
            true,
            LoaderFailure::InvalidParameter
        ));
    }

    #[test]
    fn library_only_probe_selects_win32_when_load_succeeds() {
        assert_eq!(select_api_level(true, false, false), ApiLevel::Win32);
        assert_eq!(select_api_level(false, false, false), ApiLevel::OneCore);
    }

    #[test]
    fn procedure_probe_requires_both_library_and_symbol() {
        assert_eq!(select_api_level(true, true, true), ApiLevel::Win32);
        assert_eq!(select_api_level(true, true, false), ApiLevel::OneCore);
        assert_eq!(select_api_level(false, true, true), ApiLevel::OneCore);
    }

    #[test]
    fn failed_probe_releases_only_an_acquired_module() {
        assert!(should_free_module(ApiLevel::OneCore, true));
        assert!(!should_free_module(ApiLevel::OneCore, false));
        assert!(!should_free_module(ApiLevel::Win32, true));
    }
}
