//! Deterministic keyboard-to-selection command mapping from `TerminalCore`.
//!
//! This module ports `Terminal::ConvertKeyEventToUpdateSelectionParams` without
//! depending on Win32 headers. The virtual-key values are the stable values used
//! by the Windows console input contract.

use crate::control_key_states::ControlKeyStates;
use crate::selection::SelectionInteractionMode;

/// Direction in which a keyboard selection endpoint moves.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SelectionDirection {
    Left,
    Right,
    Up,
    Down,
}

/// Expansion policy used specifically by keyboard selection movement.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KeyboardSelectionExpansion {
    Char,
    Word,
    Viewport,
    Buffer,
}

/// Platform-neutral equivalent of `Terminal::UpdateSelectionParams`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct UpdateSelectionParams {
    pub direction: SelectionDirection,
    pub expansion: KeyboardSelectionExpansion,
}

/// Windows virtual-key constants consumed by selection movement.
pub mod virtual_key {
    pub const PRIOR: u16 = 0x21;
    pub const NEXT: u16 = 0x22;
    pub const END: u16 = 0x23;
    pub const HOME: u16 = 0x24;
    pub const LEFT: u16 = 0x25;
    pub const UP: u16 = 0x26;
    pub const RIGHT: u16 = 0x27;
    pub const DOWN: u16 = 0x28;
}

/// Converts a key event into a deterministic selection movement command.
///
/// Compatibility rules preserved from `TerminalCore`:
///
/// - Selection movement is enabled in Mark Mode, or outside Mark Mode while
///   Shift is held.
/// - Alt suppresses selection movement entirely.
/// - Ctrl changes Left/Right to word movement and Home/End to whole-buffer
///   movement.
/// - Without Ctrl, Home/End and PageUp/PageDown use viewport movement while
///   the arrow keys move by character.
#[must_use]
pub fn convert_key_event_to_update_selection_params(
    interaction: SelectionInteractionMode,
    mods: ControlKeyStates,
    vkey: u16,
) -> Option<UpdateSelectionParams> {
    if !matches!(interaction, SelectionInteractionMode::Mark) && !mods.is_shift_pressed() {
        return None;
    }

    if mods.is_alt_pressed() {
        return None;
    }

    if mods.is_ctrl_pressed() {
        return match vkey {
            virtual_key::LEFT => Some(UpdateSelectionParams {
                direction: SelectionDirection::Left,
                expansion: KeyboardSelectionExpansion::Word,
            }),
            virtual_key::RIGHT => Some(UpdateSelectionParams {
                direction: SelectionDirection::Right,
                expansion: KeyboardSelectionExpansion::Word,
            }),
            virtual_key::HOME => Some(UpdateSelectionParams {
                direction: SelectionDirection::Left,
                expansion: KeyboardSelectionExpansion::Buffer,
            }),
            virtual_key::END => Some(UpdateSelectionParams {
                direction: SelectionDirection::Right,
                expansion: KeyboardSelectionExpansion::Buffer,
            }),
            _ => None,
        };
    }

    match vkey {
        virtual_key::HOME => Some(UpdateSelectionParams {
            direction: SelectionDirection::Left,
            expansion: KeyboardSelectionExpansion::Viewport,
        }),
        virtual_key::END => Some(UpdateSelectionParams {
            direction: SelectionDirection::Right,
            expansion: KeyboardSelectionExpansion::Viewport,
        }),
        virtual_key::PRIOR => Some(UpdateSelectionParams {
            direction: SelectionDirection::Up,
            expansion: KeyboardSelectionExpansion::Viewport,
        }),
        virtual_key::NEXT => Some(UpdateSelectionParams {
            direction: SelectionDirection::Down,
            expansion: KeyboardSelectionExpansion::Viewport,
        }),
        virtual_key::LEFT => Some(UpdateSelectionParams {
            direction: SelectionDirection::Left,
            expansion: KeyboardSelectionExpansion::Char,
        }),
        virtual_key::RIGHT => Some(UpdateSelectionParams {
            direction: SelectionDirection::Right,
            expansion: KeyboardSelectionExpansion::Char,
        }),
        virtual_key::UP => Some(UpdateSelectionParams {
            direction: SelectionDirection::Up,
            expansion: KeyboardSelectionExpansion::Char,
        }),
        virtual_key::DOWN => Some(UpdateSelectionParams {
            direction: SelectionDirection::Down,
            expansion: KeyboardSelectionExpansion::Char,
        }),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn shifted() -> ControlKeyStates {
        ControlKeyStates::SHIFT_PRESSED
    }

    fn shifted_ctrl() -> ControlKeyStates {
        ControlKeyStates::SHIFT_PRESSED | ControlKeyStates::LEFT_CTRL_PRESSED
    }

    #[test]
    fn ignores_plain_keys_outside_mark_mode() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                ControlKeyStates::default(),
                virtual_key::LEFT,
            ),
            None
        );
    }

    #[test]
    fn mark_mode_does_not_require_shift() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::Mark,
                ControlKeyStates::default(),
                virtual_key::LEFT,
            ),
            Some(UpdateSelectionParams {
                direction: SelectionDirection::Left,
                expansion: KeyboardSelectionExpansion::Char,
            })
        );
    }

    #[test]
    fn alt_suppresses_selection_movement() {
        let mods = shifted() | ControlKeyStates::LEFT_ALT_PRESSED;
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                mods,
                virtual_key::RIGHT,
            ),
            None
        );
    }

    #[test]
    fn ctrl_left_and_right_move_by_word() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                shifted_ctrl(),
                virtual_key::LEFT,
            ),
            Some(UpdateSelectionParams {
                direction: SelectionDirection::Left,
                expansion: KeyboardSelectionExpansion::Word,
            })
        );
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                shifted_ctrl(),
                virtual_key::RIGHT,
            ),
            Some(UpdateSelectionParams {
                direction: SelectionDirection::Right,
                expansion: KeyboardSelectionExpansion::Word,
            })
        );
    }

    #[test]
    fn ctrl_home_and_end_move_to_buffer_edges() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                shifted_ctrl(),
                virtual_key::HOME,
            ),
            Some(UpdateSelectionParams {
                direction: SelectionDirection::Left,
                expansion: KeyboardSelectionExpansion::Buffer,
            })
        );
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::None,
                shifted_ctrl(),
                virtual_key::END,
            ),
            Some(UpdateSelectionParams {
                direction: SelectionDirection::Right,
                expansion: KeyboardSelectionExpansion::Buffer,
            })
        );
    }

    #[test]
    fn home_end_and_page_keys_move_by_viewport_without_ctrl() {
        let cases = [
            (
                virtual_key::HOME,
                SelectionDirection::Left,
                KeyboardSelectionExpansion::Viewport,
            ),
            (
                virtual_key::END,
                SelectionDirection::Right,
                KeyboardSelectionExpansion::Viewport,
            ),
            (
                virtual_key::PRIOR,
                SelectionDirection::Up,
                KeyboardSelectionExpansion::Viewport,
            ),
            (
                virtual_key::NEXT,
                SelectionDirection::Down,
                KeyboardSelectionExpansion::Viewport,
            ),
        ];

        for (vkey, direction, expansion) in cases {
            assert_eq!(
                convert_key_event_to_update_selection_params(
                    SelectionInteractionMode::None,
                    shifted(),
                    vkey,
                ),
                Some(UpdateSelectionParams {
                    direction,
                    expansion,
                })
            );
        }
    }

    #[test]
    fn arrows_move_by_character_without_ctrl() {
        let cases = [
            (virtual_key::LEFT, SelectionDirection::Left),
            (virtual_key::RIGHT, SelectionDirection::Right),
            (virtual_key::UP, SelectionDirection::Up),
            (virtual_key::DOWN, SelectionDirection::Down),
        ];

        for (vkey, direction) in cases {
            assert_eq!(
                convert_key_event_to_update_selection_params(
                    SelectionInteractionMode::None,
                    shifted(),
                    vkey,
                ),
                Some(UpdateSelectionParams {
                    direction,
                    expansion: KeyboardSelectionExpansion::Char,
                })
            );
        }
    }

    #[test]
    fn ctrl_does_not_fall_back_to_non_ctrl_commands() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::Mark,
                ControlKeyStates::LEFT_CTRL_PRESSED,
                virtual_key::UP,
            ),
            None
        );
    }

    #[test]
    fn unrelated_virtual_keys_are_ignored() {
        assert_eq!(
            convert_key_event_to_update_selection_params(
                SelectionInteractionMode::Mark,
                ControlKeyStates::default(),
                0x41,
            ),
            None
        );
    }
}
