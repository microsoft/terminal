//! Pure wire-format planning for `RemoteConsoleControl`.
//!
//! The C++ boundary continues to own the signal pipe and `WriteFile`. This module only
//! produces the packed bytes that `RemoteConsoleControl::_SendTypedPacket` writes.

const NOTIFY_APP_SIGNAL: u8 = 1;
const END_TASK_SIGNAL: u8 = 7;
const NOTIFY_APP_DATA_SIZE: u32 = 8;
const END_TASK_DATA_SIZE: u32 = 16;

/// Serialize `HostSignals::NotifyApp` plus `HostSignalNotifyAppData`.
#[must_use]
pub fn notify_app_packet(process_id: u32) -> [u8; 9] {
    let mut packet = [0; 9];
    packet[0] = NOTIFY_APP_SIGNAL;
    packet[1..5].copy_from_slice(&NOTIFY_APP_DATA_SIZE.to_le_bytes());
    packet[5..9].copy_from_slice(&process_id.to_le_bytes());
    packet
}

/// Serialize `HostSignals::EndTask` plus `HostSignalEndTaskData`.
#[must_use]
pub fn end_task_packet(process_id: u32, event_type: u32, ctrl_flags: u32) -> [u8; 17] {
    let mut packet = [0; 17];
    packet[0] = END_TASK_SIGNAL;
    packet[1..5].copy_from_slice(&END_TASK_DATA_SIZE.to_le_bytes());
    packet[5..9].copy_from_slice(&process_id.to_le_bytes());
    packet[9..13].copy_from_slice(&event_type.to_le_bytes());
    packet[13..17].copy_from_slice(&ctrl_flags.to_le_bytes());
    packet
}

/// Identify methods that intentionally stay in-process instead of using the host-signal pipe.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LocalControlOperation {
    SetForeground,
    SetWindowOwner,
}

#[cfg(test)]
mod tests {
    use super::{LocalControlOperation, end_task_packet, notify_app_packet};

    #[test]
    fn notify_app_matches_packed_cpp_wire_layout() {
        assert_eq!(
            notify_app_packet(0x1234_5678),
            [1, 8, 0, 0, 0, 0x78, 0x56, 0x34, 0x12]
        );
    }

    #[test]
    fn end_task_matches_packed_cpp_wire_layout() {
        assert_eq!(
            end_task_packet(0x1122_3344, 0x5566_7788, 0x99aa_bbcc),
            [
                7, 16, 0, 0, 0, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0xcc, 0xbb, 0xaa,
                0x99,
            ]
        );
    }

    #[test]
    fn local_operations_are_not_pipe_packets() {
        assert_ne!(
            LocalControlOperation::SetForeground,
            LocalControlOperation::SetWindowOwner
        );
    }
}
