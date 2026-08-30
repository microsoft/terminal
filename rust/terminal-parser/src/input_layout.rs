//! Keyboard-layout enrichment for VT input records.
//!
//! Microsoft's input parser derives the virtual key, scan code, and any layout-
//! required Shift state for C0 controls with `VkKeyScanW`/`MapVirtualKeyW`.
//! The parser itself remains platform neutral: this dispatch decorator owns the
//! seam where a Windows adapter can inject those lookups without leaking Win32
//! calls into the state machine.

use crate::input_engine::{InputAction, InputDispatch, InputRecord, KeyEvent, SHIFT_PRESSED};

const VK_SHIFT: u16 = 0x10;
const VK_CONTROL: u16 = 0x11;
const VK_MENU: u16 = 0x12;
const VK_BACK: u16 = 0x08;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KeyLayoutMapping {
    pub virtual_key: u16,
    pub scan_code: u16,
    pub shift_required: bool,
}

/// Platform seam corresponding to Microsoft's `VkKeyScanW` plus
/// `MapVirtualKeyW(..., MAPVK_VK_TO_VSC)` pair.
pub trait KeyboardLayoutMapper {
    fn map_character(&self, character: u16) -> Option<KeyLayoutMapping>;
}

/// Decorates an [`InputDispatch`] with keyboard-layout-derived C0 key fields.
///
/// `InputStateMachineEngine` continues to own VT semantics. This layer only
/// enriches the main C0 key records with the layout-dependent fields that the
/// Microsoft unit contract obtains from Win32. Modifier key records and the
/// dedicated Ctrl+C `WriteCtrlKey` path are intentionally left untouched.
pub struct LayoutMappedInputDispatch<D, M> {
    inner: D,
    mapper: M,
}

impl<D, M> LayoutMappedInputDispatch<D, M> {
    #[must_use]
    pub const fn new(inner: D, mapper: M) -> Self {
        Self { inner, mapper }
    }

    #[must_use]
    pub const fn inner(&self) -> &D {
        &self.inner
    }

    pub const fn inner_mut(&mut self) -> &mut D {
        &mut self.inner
    }

    #[must_use]
    pub fn into_inner(self) -> D {
        self.inner
    }
}

impl<D: InputDispatch, M: KeyboardLayoutMapper> InputDispatch for LayoutMappedInputDispatch<D, M> {
    fn dispatch(&mut self, action: InputAction) {
        let action = match action {
            InputAction::WriteInput(mut records) => {
                for record in &mut records {
                    if let InputRecord::Key(key) = record {
                        enrich_c0_key(key, &self.mapper);
                    }
                }
                InputAction::WriteInput(records)
            }
            other => other,
        };
        self.inner.dispatch(action);
    }

    fn is_vt_input_enabled(&self) -> bool {
        self.inner.is_vt_input_enabled()
    }
}

fn enrich_c0_key<M: KeyboardLayoutMapper>(key: &mut KeyEvent, mapper: &M) {
    if matches!(key.virtual_key, VK_SHIFT | VK_CONTROL | VK_MENU) {
        return;
    }

    let source_character =
        if key.unicode_char == 0x7f && key.virtual_key == VK_BACK && key.control_key_state != 0 {
            // Microsoft's C0 test sends DEL for Ctrl+Backspace but performs the
            // layout lookup with the original backspace character.
            0x08
        } else if key.unicode_char < 0x20 {
            key.unicode_char
        } else {
            return;
        };

    let Some(mapping) = mapper.map_character(source_character) else {
        return;
    };

    key.virtual_key = mapping.virtual_key;
    key.scan_code = mapping.scan_code;
    if mapping.shift_required {
        key.control_key_state |= SHIFT_PRESSED;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::input_engine::{InputStateMachineEngine, LEFT_CTRL_PRESSED};
    use crate::state_machine::StateMachine;

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

    /// Deterministic fixture for the US keyboard layout used by Microsoft's
    /// C0 contract. A Windows production adapter can implement the same trait
    /// with the live `VkKeyScanW`/`MapVirtualKeyW` results.
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
                    let scan_code = letter_scan_code(virtual_key)?;
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

    fn letter_scan_code(virtual_key: u16) -> Option<u16> {
        Some(match u8::try_from(virtual_key).ok()? {
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
        })
    }

    fn expected_main_key(code_unit: u16) -> (u16, u16, u16, u32) {
        let sent_character = if code_unit == 0x08 { 0x7f } else { code_unit };
        let write_ctrl = !matches!(code_unit, 0x09 | 0x0d | 0x1b);
        let mapping = MicrosoftUsC0Mapper
            .map_character(code_unit)
            .expect("all Microsoft C0 fixture characters map");
        let mut modifiers = if write_ctrl { LEFT_CTRL_PRESSED } else { 0 };
        if mapping.shift_required {
            modifiers |= SHIFT_PRESSED;
        }
        (
            mapping.virtual_key,
            mapping.scan_code,
            sent_character,
            modifiers,
        )
    }

    #[test]
    fn microsoft_input_engine_c0_full_layout_seam_contract() {
        for code_unit in 0_u16..0x20 {
            let dispatch =
                LayoutMappedInputDispatch::new(RecordingDispatch::default(), MicrosoftUsC0Mapper);
            let mut machine = StateMachine::new_input(InputStateMachineEngine::new(dispatch));
            machine.process_utf16(&[code_unit]);

            if code_unit == 0x03 {
                let actions = &machine.engine().dispatch().inner().actions;
                assert_eq!(actions.len(), 2);
                assert!(matches!(
                    actions[0],
                    InputAction::WriteCtrlKey(key)
                        if key.key_down
                            && key.virtual_key == u16::from(b'C')
                            && key.scan_code == 0
                            && key.unicode_char == 0x03
                            && key.control_key_state == LEFT_CTRL_PRESSED
                ));
                continue;
            }

            if code_unit == 0x1b {
                // Microsoft resolves a trailing ESC at the ProcessString chunk
                // boundary. Rust does not own that heuristic yet, so keep this
                // contract Partial while still proving that the layout seam has
                // the correct Escape mapping available for the eventual fix.
                assert!(machine.engine().dispatch().inner().actions.is_empty());
                assert_eq!(
                    MicrosoftUsC0Mapper.map_character(code_unit),
                    Some(KeyLayoutMapping {
                        virtual_key: VK_ESCAPE,
                        scan_code: 0x01,
                        shift_required: false,
                    })
                );
                continue;
            }

            let expected = expected_main_key(code_unit);
            let main_key = machine
                .engine()
                .dispatch()
                .inner()
                .actions
                .iter()
                .find_map(|action| match action {
                    InputAction::WriteInput(records) => {
                        records.iter().find_map(|record| match record {
                            InputRecord::Key(key)
                                if key.key_down
                                    && !matches!(
                                        key.virtual_key,
                                        VK_SHIFT | VK_CONTROL | VK_MENU
                                    ) =>
                            {
                                Some(*key)
                            }
                            _ => None,
                        })
                    }
                    _ => None,
                })
                .expect("portable Microsoft C0 vector emits a main key record");

            assert_eq!(
                (
                    main_key.virtual_key,
                    main_key.scan_code,
                    main_key.unicode_char,
                    main_key.control_key_state,
                ),
                expected,
                "C0 code unit {code_unit:#04x}"
            );
        }
    }
}
