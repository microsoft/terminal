//! Pure fallback decisions for the OneCore-safe VT API wrappers.
//!
//! Win32 calls, `GetLastError`, and `ConIoSrvComm` ownership remain at the platform boundary.

/// API wrapper whose native result may trigger a `OneCore` fallback.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RedirectedApi {
    MapVirtualKey,
    VkKeyScan,
    GetKeyState,
}

/// Failure classification supplied by the Win32 boundary.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum NativeFailure {
    ProcedureNotFound,
    DelayLoadFailed,
    Other,
}

/// Decide whether the `OneCore` service should be consulted after the native call.
///
/// The native sentinel differs by API: `MapVirtualKeyW` and `GetKeyState` use zero,
/// while `VkKeyScanW` uses -1.
#[must_use]
pub const fn should_use_onecore_fallback(
    api: RedirectedApi,
    native_result: i32,
    failure: NativeFailure,
) -> bool {
    let hit_sentinel = match api {
        RedirectedApi::MapVirtualKey | RedirectedApi::GetKeyState => native_result == 0,
        RedirectedApi::VkKeyScan => native_result == -1,
    };

    hit_sentinel
        && matches!(
            failure,
            NativeFailure::ProcedureNotFound | NativeFailure::DelayLoadFailed
        )
}

#[cfg(test)]
mod tests {
    use super::{NativeFailure, RedirectedApi, should_use_onecore_fallback};

    #[test]
    fn map_virtual_key_uses_zero_as_failure_sentinel() {
        assert!(should_use_onecore_fallback(
            RedirectedApi::MapVirtualKey,
            0,
            NativeFailure::ProcedureNotFound
        ));
        assert!(!should_use_onecore_fallback(
            RedirectedApi::MapVirtualKey,
            1,
            NativeFailure::ProcedureNotFound
        ));
    }

    #[test]
    fn vk_key_scan_uses_minus_one_as_failure_sentinel() {
        assert!(should_use_onecore_fallback(
            RedirectedApi::VkKeyScan,
            -1,
            NativeFailure::DelayLoadFailed
        ));
        assert!(!should_use_onecore_fallback(
            RedirectedApi::VkKeyScan,
            0,
            NativeFailure::DelayLoadFailed
        ));
    }

    #[test]
    fn get_key_state_uses_zero_as_failure_sentinel() {
        assert!(should_use_onecore_fallback(
            RedirectedApi::GetKeyState,
            0,
            NativeFailure::DelayLoadFailed
        ));
        assert!(!should_use_onecore_fallback(
            RedirectedApi::GetKeyState,
            -1,
            NativeFailure::DelayLoadFailed
        ));
    }

    #[test]
    fn unrelated_native_failures_never_redirect() {
        for api in [
            RedirectedApi::MapVirtualKey,
            RedirectedApi::VkKeyScan,
            RedirectedApi::GetKeyState,
        ] {
            let sentinel = if api == RedirectedApi::VkKeyScan {
                -1
            } else {
                0
            };
            assert!(!should_use_onecore_fallback(
                api,
                sentinel,
                NativeFailure::Other
            ));
        }
    }
}
