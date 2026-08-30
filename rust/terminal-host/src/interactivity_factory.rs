//! Pure implementation selection for `InteractivityFactory`.

use crate::api_detection::ApiLevel;

/// Factory product whose platform implementation is selected from the detected API level.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FactoryProduct {
    ConsoleControl,
    ConsoleInputThread,
    HighDpiApi,
    WindowMetrics,
    SystemConfigurationProvider,
    PseudoWindow,
}

/// Platform implementation selected by the factory.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FactoryImplementation {
    Win32,
    OneCore,
    None,
}

/// Factory selection failure corresponding to the C++ `STATUS_INVALID_LEVEL` path.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct UnsupportedApiLevel;

/// Select the same implementation family as `InteractivityFactory` without constructing it.
///
/// # Errors
/// Returns [`UnsupportedApiLevel`] when the detected API level is `OneCore` but the build
/// does not include `OneCore` interactivity support.
pub const fn select_factory_implementation(
    product: FactoryProduct,
    level: ApiLevel,
    onecore_enabled: bool,
) -> Result<FactoryImplementation, UnsupportedApiLevel> {
    match level {
        ApiLevel::Win32 => Ok(FactoryImplementation::Win32),
        ApiLevel::OneCore if !onecore_enabled => Err(UnsupportedApiLevel),
        ApiLevel::OneCore => match product {
            FactoryProduct::HighDpiApi | FactoryProduct::PseudoWindow => {
                Ok(FactoryImplementation::None)
            }
            FactoryProduct::ConsoleControl
            | FactoryProduct::ConsoleInputThread
            | FactoryProduct::WindowMetrics
            | FactoryProduct::SystemConfigurationProvider => Ok(FactoryImplementation::OneCore),
        },
    }
}

#[cfg(test)]
mod tests {
    use crate::api_detection::ApiLevel;

    use super::{FactoryImplementation, FactoryProduct, select_factory_implementation};

    #[test]
    fn win32_level_selects_win32_for_every_product() {
        for product in [
            FactoryProduct::ConsoleControl,
            FactoryProduct::ConsoleInputThread,
            FactoryProduct::HighDpiApi,
            FactoryProduct::WindowMetrics,
            FactoryProduct::SystemConfigurationProvider,
            FactoryProduct::PseudoWindow,
        ] {
            assert_eq!(
                select_factory_implementation(product, ApiLevel::Win32, false),
                Ok(FactoryImplementation::Win32)
            );
        }
    }

    #[test]
    fn onecore_selects_platform_services_but_not_high_dpi_or_pseudo_window() {
        for product in [
            FactoryProduct::ConsoleControl,
            FactoryProduct::ConsoleInputThread,
            FactoryProduct::WindowMetrics,
            FactoryProduct::SystemConfigurationProvider,
        ] {
            assert_eq!(
                select_factory_implementation(product, ApiLevel::OneCore, true),
                Ok(FactoryImplementation::OneCore)
            );
        }

        assert_eq!(
            select_factory_implementation(FactoryProduct::HighDpiApi, ApiLevel::OneCore, true),
            Ok(FactoryImplementation::None)
        );
        assert_eq!(
            select_factory_implementation(FactoryProduct::PseudoWindow, ApiLevel::OneCore, true),
            Ok(FactoryImplementation::None)
        );
    }

    #[test]
    fn onecore_level_is_invalid_when_onecore_build_support_is_absent() {
        assert!(
            select_factory_implementation(FactoryProduct::ConsoleControl, ApiLevel::OneCore, false)
                .is_err()
        );
    }
}
