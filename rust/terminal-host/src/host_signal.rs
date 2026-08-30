//! Pure framing for the terminal-to-host signal stream.
//!
//! `HostSignalInputThread` keeps ownership of the Windows pipe, shutdown path,
//! and `IConsoleControl` calls. This module captures the byte-level contract and
//! typed-packet validation without platform I/O.

const NOTIFY_APP: u8 = 1;
const SET_FOREGROUND: u8 = 5;
const END_TASK: u8 = 7;

const NOTIFY_APP_SIZE: usize = 8;
const SET_FOREGROUND_SIZE: usize = 12;
const END_TASK_SIZE: usize = 16;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HostSignalAction {
    NotifyApp {
        process_id: u32,
    },
    /// Retained for compatibility with older terminals; current conhost ignores it.
    SetForeground {
        process_handle: u32,
        is_foreground: bool,
    },
    EndTask {
        process_id: u32,
        event_type: u32,
        ctrl_flags: u32,
    },
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HostSignalDecodeError {
    UnknownSignal(u8),
    TruncatedPacket,
    PacketSmallerThanKnownType,
}

/// Decodes a complete in-memory host-signal stream.
///
/// Typed packets are self-sized. A declared size may exceed the known C++
/// structure size; those extension bytes are skipped before reading the next
/// signal, matching `_ReceiveTypedPacket`.
///
/// # Errors
/// Returns an error for an unknown signal ID, a packet shorter than its known
/// C++ structure, or a stream that ends before the declared packet size.
pub fn decode_host_signal_stream(
    bytes: &[u8],
) -> Result<Vec<HostSignalAction>, HostSignalDecodeError> {
    let mut cursor = 0;
    let mut actions = Vec::new();

    while cursor < bytes.len() {
        let signal = bytes[cursor];
        cursor += 1;

        let minimum_size = match signal {
            NOTIFY_APP => NOTIFY_APP_SIZE,
            SET_FOREGROUND => SET_FOREGROUND_SIZE,
            END_TASK => END_TASK_SIZE,
            other => return Err(HostSignalDecodeError::UnknownSignal(other)),
        };

        let declared_size =
            read_u32(bytes, cursor).ok_or(HostSignalDecodeError::TruncatedPacket)? as usize;
        if declared_size < minimum_size {
            return Err(HostSignalDecodeError::PacketSmallerThanKnownType);
        }

        let packet = bytes
            .get(cursor..cursor + declared_size)
            .ok_or(HostSignalDecodeError::TruncatedPacket)?;

        let action = match signal {
            NOTIFY_APP => HostSignalAction::NotifyApp {
                process_id: read_u32(packet, 4).ok_or(HostSignalDecodeError::TruncatedPacket)?,
            },
            SET_FOREGROUND => HostSignalAction::SetForeground {
                process_handle: read_u32(packet, 4)
                    .ok_or(HostSignalDecodeError::TruncatedPacket)?,
                is_foreground: packet
                    .get(8)
                    .copied()
                    .ok_or(HostSignalDecodeError::TruncatedPacket)?
                    != 0,
            },
            END_TASK => HostSignalAction::EndTask {
                process_id: read_u32(packet, 4).ok_or(HostSignalDecodeError::TruncatedPacket)?,
                event_type: read_u32(packet, 8).ok_or(HostSignalDecodeError::TruncatedPacket)?,
                ctrl_flags: read_u32(packet, 12).ok_or(HostSignalDecodeError::TruncatedPacket)?,
            },
            _ => return Err(HostSignalDecodeError::UnknownSignal(signal)),
        };

        actions.push(action);
        cursor += declared_size;
    }

    Ok(actions)
}

fn read_u32(bytes: &[u8], offset: usize) -> Option<u32> {
    let slice = bytes.get(offset..offset + 4)?;
    let [a, b, c, d] = slice else {
        return None;
    };
    Some(u32::from_le_bytes([*a, *b, *c, *d]))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn append_u32(bytes: &mut Vec<u8>, value: u32) {
        bytes.extend_from_slice(&value.to_le_bytes());
    }

    #[test]
    fn canonical_signal_ids_and_payloads_decode_in_order() {
        let mut bytes = vec![NOTIFY_APP];
        append_u32(&mut bytes, 8);
        append_u32(&mut bytes, 42);

        bytes.push(SET_FOREGROUND);
        append_u32(&mut bytes, 12);
        append_u32(&mut bytes, 0x1234_5678);
        bytes.extend_from_slice(&[1, 0, 0, 0]);

        bytes.push(END_TASK);
        append_u32(&mut bytes, 16);
        append_u32(&mut bytes, 77);
        append_u32(&mut bytes, 2);
        append_u32(&mut bytes, 9);

        assert_eq!(
            decode_host_signal_stream(&bytes).unwrap(),
            vec![
                HostSignalAction::NotifyApp { process_id: 42 },
                HostSignalAction::SetForeground {
                    process_handle: 0x1234_5678,
                    is_foreground: true,
                },
                HostSignalAction::EndTask {
                    process_id: 77,
                    event_type: 2,
                    ctrl_flags: 9,
                },
            ]
        );
    }

    #[test]
    fn larger_typed_packet_skips_extension_bytes() {
        let mut bytes = vec![NOTIFY_APP];
        append_u32(&mut bytes, 12);
        append_u32(&mut bytes, 7);
        bytes.extend_from_slice(&[0xaa, 0xbb, 0xcc, 0xdd]);
        bytes.push(NOTIFY_APP);
        append_u32(&mut bytes, 8);
        append_u32(&mut bytes, 8);

        assert_eq!(
            decode_host_signal_stream(&bytes).unwrap(),
            vec![
                HostSignalAction::NotifyApp { process_id: 7 },
                HostSignalAction::NotifyApp { process_id: 8 },
            ]
        );
    }

    #[test]
    fn undersized_typed_packet_is_rejected() {
        let mut bytes = vec![END_TASK];
        append_u32(&mut bytes, 12);
        bytes.extend_from_slice(&[0; 8]);

        assert_eq!(
            decode_host_signal_stream(&bytes),
            Err(HostSignalDecodeError::PacketSmallerThanKnownType)
        );
    }

    #[test]
    fn truncated_declared_packet_is_rejected() {
        let mut bytes = vec![NOTIFY_APP];
        append_u32(&mut bytes, 8);
        bytes.extend_from_slice(&[1, 2]);

        assert_eq!(
            decode_host_signal_stream(&bytes),
            Err(HostSignalDecodeError::TruncatedPacket)
        );
    }

    #[test]
    fn unknown_signal_is_rejected_immediately() {
        assert_eq!(
            decode_host_signal_stream(&[99]),
            Err(HostSignalDecodeError::UnknownSignal(99))
        );
    }
}
