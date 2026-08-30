//! Deterministic request validation from `src/server/ApiSorter.cpp`.
//!
//! This module intentionally stops before invoking a server API routine. The
//! C++ dispatcher still owns function pointers, exception translation, NTSTATUS
//! conversion, pending replies, and device I/O.

/// Number of API entries in each of the three canonical console API layers.
pub const CONSOLE_API_LAYER_COUNTS: [usize; 3] = [10, 22, 45];

/// Decomposed console API number.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ApiAddress {
    pub layer_index: usize,
    pub api_index: usize,
}

/// Offsets and reply-buffer state initialized before a server routine runs.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DispatchPlan {
    pub address: ApiAddress,
    pub write_size: usize,
    pub write_offset: usize,
    pub read_offset: usize,
}

/// Reason a console API request cannot be dispatched.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum DispatchValidationError {
    InvalidLayer,
    InvalidApi,
    InputSmallerThanHeader,
    DescriptorExceedsMessageUnion,
    DescriptorExceedsInput,
    DescriptorSmallerThanRequired,
}

/// Splits the encoded API number into its zero-based layer and API indices.
///
/// C++ encodes the one-based layer in the high byte and the API index in the
/// low 24 bits.
///
/// # Errors
/// Returns `InvalidLayer` for an unknown layer and `InvalidApi` when the low
/// 24-bit index is outside that layer's canonical table.
pub fn decode_api_number(api_number: u32) -> Result<ApiAddress, DispatchValidationError> {
    let encoded_layer = (api_number >> 24) as usize;
    if encoded_layer == 0 || encoded_layer > CONSOLE_API_LAYER_COUNTS.len() {
        return Err(DispatchValidationError::InvalidLayer);
    }

    let address = ApiAddress {
        layer_index: encoded_layer - 1,
        api_index: (api_number & 0x00ff_ffff) as usize,
    };

    if address.api_index >= CONSOLE_API_LAYER_COUNTS[address.layer_index] {
        return Err(DispatchValidationError::InvalidApi);
    }

    Ok(address)
}

/// Validates the pure size contract and prepares the same offsets initialized
/// by `ApiSorter::ConsoleDispatchRequest` before it calls the API routine.
///
/// # Errors
/// Returns the corresponding validation error when the API address is invalid,
/// the input cannot contain its header and descriptor, the descriptor exceeds
/// the message union, or the descriptor is smaller than the selected API's
/// required structure.
pub fn plan_dispatch(
    api_number: u32,
    input_size: usize,
    api_descriptor_size: usize,
    required_descriptor_size: usize,
    header_size: usize,
    message_union_capacity: usize,
) -> Result<DispatchPlan, DispatchValidationError> {
    let address = decode_api_number(api_number)?;

    if input_size < header_size {
        return Err(DispatchValidationError::InputSmallerThanHeader);
    }
    if api_descriptor_size > message_union_capacity {
        return Err(DispatchValidationError::DescriptorExceedsMessageUnion);
    }

    let payload_capacity = input_size - header_size;
    if api_descriptor_size > payload_capacity {
        return Err(DispatchValidationError::DescriptorExceedsInput);
    }
    if api_descriptor_size < required_descriptor_size {
        return Err(DispatchValidationError::DescriptorSmallerThanRequired);
    }

    Ok(DispatchPlan {
        address,
        write_size: api_descriptor_size,
        write_offset: api_descriptor_size,
        read_offset: api_descriptor_size + header_size,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    const HEADER: usize = 16;
    const UNION: usize = 256;

    fn encoded(layer: u32, api: u32) -> u32 {
        (layer << 24) | api
    }

    #[test]
    fn canonical_layer_boundaries_are_accepted() {
        assert_eq!(
            decode_api_number(encoded(1, 9)).unwrap(),
            ApiAddress {
                layer_index: 0,
                api_index: 9,
            }
        );
        assert_eq!(
            decode_api_number(encoded(2, 21)).unwrap(),
            ApiAddress {
                layer_index: 1,
                api_index: 21,
            }
        );
        assert_eq!(
            decode_api_number(encoded(3, 44)).unwrap(),
            ApiAddress {
                layer_index: 2,
                api_index: 44,
            }
        );
    }

    #[test]
    fn zero_and_unknown_layers_are_rejected() {
        assert_eq!(
            decode_api_number(encoded(0, 0)),
            Err(DispatchValidationError::InvalidLayer)
        );
        assert_eq!(
            decode_api_number(encoded(4, 0)),
            Err(DispatchValidationError::InvalidLayer)
        );
    }

    #[test]
    fn api_index_must_fit_selected_layer() {
        assert_eq!(
            decode_api_number(encoded(1, 10)),
            Err(DispatchValidationError::InvalidApi)
        );
        assert_eq!(
            decode_api_number(encoded(2, 22)),
            Err(DispatchValidationError::InvalidApi)
        );
        assert_eq!(
            decode_api_number(encoded(3, 45)),
            Err(DispatchValidationError::InvalidApi)
        );
    }

    #[test]
    fn dispatch_plan_matches_cpp_offset_initialization() {
        assert_eq!(
            plan_dispatch(encoded(2, 5), 80, 32, 24, HEADER, UNION).unwrap(),
            DispatchPlan {
                address: ApiAddress {
                    layer_index: 1,
                    api_index: 5,
                },
                write_size: 32,
                write_offset: 32,
                read_offset: 48,
            }
        );
    }

    #[test]
    fn each_cpp_size_guard_has_a_distinct_failure() {
        assert_eq!(
            plan_dispatch(encoded(1, 0), 15, 0, 0, HEADER, UNION),
            Err(DispatchValidationError::InputSmallerThanHeader)
        );
        assert_eq!(
            plan_dispatch(encoded(1, 0), 400, 257, 0, HEADER, UNION),
            Err(DispatchValidationError::DescriptorExceedsMessageUnion)
        );
        assert_eq!(
            plan_dispatch(encoded(1, 0), 40, 25, 0, HEADER, UNION),
            Err(DispatchValidationError::DescriptorExceedsInput)
        );
        assert_eq!(
            plan_dispatch(encoded(1, 0), 80, 23, 24, HEADER, UNION),
            Err(DispatchValidationError::DescriptorSmallerThanRequired)
        );
    }

    #[test]
    fn exact_boundaries_are_valid() {
        assert!(plan_dispatch(encoded(3, 44), 40, 24, 24, HEADER, 24).is_ok());
    }
}
