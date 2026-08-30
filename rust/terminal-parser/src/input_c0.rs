//! Input-chunk compatibility for Microsoft's C0 parser contract.
//!
//! Microsoft's input-side state machine resolves a trailing ESC at a
//! `ProcessString` boundary as the Escape key instead of keeping an incomplete
//! VT sequence pending. The portable parser keeps sequence state across chunks,
//! so this adapter supplies the input-specific chunk-boundary behavior without
//! changing output-parser semantics.

use crate::input_engine::{
    InputAction, InputDispatch, InputRecord, InputStateMachineEngine, KeyEvent,
};
use crate::state_machine::{State, StateMachine};

const ESC: u16 = 0x1b;

/// Processes an input chunk and applies Microsoft's input-only trailing-ESC
/// boundary rule.
pub fn process_input_utf16_chunk<D: InputDispatch>(
    machine: &mut StateMachine<InputStateMachineEngine<D>>,
    text: &[u16],
) {
    machine.process_utf16(text);

    if text.last().copied() == Some(ESC) && machine.state() == State::Escape {
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(InputAction::WriteInput(vec![InputRecord::Key(KeyEvent {
                key_down: true,
                repeat_count: 1,
                virtual_key: ESC,
                scan_code: 0,
                unicode_char: ESC,
                control_key_state: 0,
            })]));
        machine.reset_state();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::input_engine::{LEFT_CTRL_PRESSED, SHIFT_PRESSED};
    use crate::input_layout::{KeyLayoutMapping, KeyboardLayoutMapper, LayoutMappedInputDispatch};

    const VK_BACK: u16 = 0x08;
    const VK_TAB: u16 = 0x09;
    const VK_RETURN: u16 = 0x0d;
    const VK_ESCAPE: u16 = 0x1b;
    const VK_OEM_MINUS: u16 = 0xbd;
    const VK_OEM_5: u16 = 0xdc;
    const VK_OEM_6: u16 = 0xdd;

    #[derive(Debug, Default)]
    struct RecordingDispatch {
        actions: Vec<InputAction>,
    }

    impl InputDispatch for RecordingDispatch {
        fn dispatch(&mut self, action: InputAction) {
            self.actions.push(action);
        }
    }

    #[derive(Debug, Clone, Copy)]
    struct MicrosoftUsC0Mapper;

    impl KeyboardLayoutMapper for MicrosoftUsC0Mapper {
        fn map_character(&self, character: u16) -> Option<KeyLayoutMapping> {
            let (virtual_key, scan_code, shift_required) = match character {
                0x00 => (u16::from(b'2'), 0x03, true),
                0x08 => (VK_BACK, 0x0e, false),
                0x09 => (VK_TAB, 0x0f, false),
                0x0d => (VK_RETURN, 0x1c, false),
                0x1b => (VK_ESCAPE, 0x01, false),
                0x1c => (VK_OEM_5, 0x2b, false),
                0x1d => (VK_OEM_6, 0x1b, false),
                0x1e => (u16::from(b'6'), 0x07, true),
                0x1f => (VK_OEM_MINUS, 0x0c, true),
                0x01..=0x1a => {
                    let virtual_key = u16::from(b'A') + character - 1;
                    let scan_code = match u8::try_from(virtual_key).ok()? {
                        b'A' => 0x1e,
                        b'B' => 0x30,
                        b'C' => 0x2e,
                        b'D' => 0x20,
                        b'E' => 0x12,
                        b'F' => 0x21,
                        b'G' => 0x22,
                        b'H' => 0x23,
                        b'I' => 0x17,
                        b'J' => 0x24,
                        b'K' => 0x25,
                        b'L' => 0x26,
                        b'M' => 0x32,
                        b'N' => 0x31,
                        b'O' => 0x18,
                        b'P' => 0x19,
                        b'Q' => 0x10,
                        b'R' => 0x13,
                        b'S' => 0x1f,
                        b'T' => 0x14,
                        b'U' => 0x16,
                        b'V' => 0x2f,
                        b'W' => 0x11,
                        b'X' => 0x2d,
                        b'Y' => 0x15,
                        b'Z' => 0x2c,
                        _ => return None,
                    };
                    (virtual_key, scan_code, false)
                }
                _ => return None,
            };
            Some(KeyLayoutMapping {
                virtual_key,
                scan_code,
                shift_required,
            })
        }
    }

    fn expected(code_unit: u16) -> (u16, u16, u16, u32) {
        if code_unit == 0x03 {
            return (u16::from(b'C'), 0, 0x03, LEFT_CTRL_PRESSED);
        }
        let mapping = MicrosoftUsC0Mapper
            .map_character(code_unit)
            .expect("all Microsoft C0 vectors map on US layout");
        let mut modifiers = if matches!(code_unit, 0x09 | 0x0d | 0x1b) {
            0
        } else {
            LEFT_CTRL_PRESSED
        };
        if mapping.shift_required {
            modifiers |= SHIFT_PRESSED;
        }
        (
            mapping.virtual_key,
            mapping.scan_code,
            if code_unit == 0x08 { 0x7f } else { code_unit },
            modifiers,
        )
    }

    #[test]
    fn microsoft_input_engine_c0_test_matches_all_32_source_vectors() {
        for code_unit in 0_u16..0x20 {
            let dispatch =
                LayoutMappedInputDispatch::new(RecordingDispatch::default(), MicrosoftUsC0Mapper);
            let mut machine = StateMachine::new_input(InputStateMachineEngine::new(dispatch));
            process_input_utf16_chunk(&mut machine, &[code_unit]);

            let actions = &machine.engine().dispatch().inner().actions;
            let main_key = if code_unit == 0x03 {
                actions.iter().find_map(|action| match action {
                    InputAction::WriteCtrlKey(key) if key.key_down => Some(*key),
                    _ => None,
                })
            } else {
                actions.iter().find_map(|action| match action {
                    InputAction::WriteInput(records) => {
                        records.iter().find_map(|record| match record {
                            InputRecord::Key(key)
                                if key.key_down && !matches!(key.virtual_key, 0x10..=0x12) =>
                            {
                                Some(*key)
                            }
                            _ => None,
                        })
                    }
                    _ => None,
                })
            }
            .expect("each Microsoft C0 vector emits one main key");

            assert_eq!(
                (
                    main_key.virtual_key,
                    main_key.scan_code,
                    main_key.unicode_char,
                    main_key.control_key_state,
                ),
                expected(code_unit),
                "C0 code unit {code_unit:#04x}"
            );
            assert_eq!(machine.state(), State::Ground);
        }
    }
}
