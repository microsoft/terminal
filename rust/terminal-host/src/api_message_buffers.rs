//! Pure buffer-planning rules from the console server's `_CONSOLE_API_MSG`.
//!
//! Allocation ownership and `DeviceComm` I/O remain C++ boundaries. This
//! module captures the deterministic offset validation, remaining-size,
//! capacity-trimming, and output-release decisions used by `ApiMessage.cpp`.

pub const LARGE_BUFFER_THRESHOLD: usize = 128 * 1024;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ApiBufferPlanError {
    OffsetPastEnd { offset: usize, total: usize },
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ApiBufferPlan {
    pub size: usize,
    pub shrink_capacity_first: bool,
}

/// Plans the lazily materialized remainder of an API message buffer.
///
/// # Errors
/// Returns [`ApiBufferPlanError::OffsetPastEnd`] when the current read/write
/// offset exceeds the descriptor's total buffer size.
pub fn plan_buffer(
    total_size: usize,
    offset: usize,
    current_capacity: usize,
) -> Result<ApiBufferPlan, ApiBufferPlanError> {
    if offset > total_size {
        return Err(ApiBufferPlanError::OffsetPastEnd {
            offset,
            total: total_size,
        });
    }

    let size = total_size - offset;
    Ok(ApiBufferPlan {
        size,
        shrink_capacity_first: current_capacity > LARGE_BUFFER_THRESHOLD
            && (current_capacity >> 1) > size,
    })
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct OutputReleasePlan {
    pub write_output: bool,
    pub offset: usize,
    pub size: usize,
}

/// Plans the side effect performed when an output buffer is released.
///
/// The C++ server writes only for a successful NTSTATUS and uses the message's
/// write offset plus the completion-information byte count. Regardless of
/// success, the owned output buffer is then cleared by the platform layer.
#[must_use]
pub const fn plan_output_release(
    output_buffer_present: bool,
    ntstatus_succeeded: bool,
    write_offset: usize,
    reply_information: usize,
) -> OutputReleasePlan {
    OutputReleasePlan {
        write_output: output_buffer_present && ntstatus_succeeded,
        offset: write_offset,
        size: reply_information,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn input_or_output_remainder_starts_at_current_offset() {
        assert_eq!(
            plan_buffer(100, 24, 0),
            Ok(ApiBufferPlan {
                size: 76,
                shrink_capacity_first: false,
            })
        );
        assert_eq!(
            plan_buffer(100, 100, 0),
            Ok(ApiBufferPlan {
                size: 0,
                shrink_capacity_first: false,
            })
        );
    }

    #[test]
    fn offset_past_descriptor_size_is_rejected() {
        assert_eq!(
            plan_buffer(99, 100, 0),
            Err(ApiBufferPlanError::OffsetPastEnd {
                offset: 100,
                total: 99,
            })
        );
    }

    #[test]
    fn oversized_capacity_is_trimmed_only_when_more_than_half_is_unneeded() {
        let capacity = LARGE_BUFFER_THRESHOLD + 2;
        assert!(
            !plan_buffer(capacity, 0, capacity)
                .unwrap()
                .shrink_capacity_first
        );
        assert!(plan_buffer(10, 0, capacity).unwrap().shrink_capacity_first);
        assert!(
            !plan_buffer(10, 0, LARGE_BUFFER_THRESHOLD)
                .unwrap()
                .shrink_capacity_first
        );
    }

    #[test]
    fn successful_release_writes_exact_completion_information() {
        assert_eq!(
            plan_output_release(true, true, 12, 34),
            OutputReleasePlan {
                write_output: true,
                offset: 12,
                size: 34,
            }
        );
    }

    #[test]
    fn failed_status_or_missing_buffer_suppresses_device_write() {
        assert!(!plan_output_release(true, false, 12, 34).write_output);
        assert!(!plan_output_release(false, true, 12, 34).write_output);
    }
}
