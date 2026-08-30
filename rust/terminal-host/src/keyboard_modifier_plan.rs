//! Pure modifier planning from `SynthesizeKeyboardEvents`.
//!
//! `VkKeyScanW`, `MapVirtualKeyW`, scan codes, and `INPUT_RECORD` construction
//! remain platform-owned. This module preserves the deterministic interpretation
//! of the modifier byte and the ordering rules around the synthesized character.

const SHIFT_BIT: u8 = 1;
const CTRL_BIT: u8 = 2;
const ALT_BIT: u8 = 4;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ModifierEnvelope {
    None,
    Shift,
    AltGr,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CharacterModifierPlan {
    pub shift_pressed: bool,
    pub left_ctrl_pressed: bool,
    pub right_alt_pressed: bool,
    pub envelope: ModifierEnvelope,
}

/// Interprets the high-byte modifier state returned by the keyboard-layout
/// lookup used by the C++ event synthesizer.
///
/// `AltGr` is represented by Ctrl+Alt together. It takes precedence over the
/// separate Shift envelope, exactly matching `SynthesizeKeyboardEvents`; Shift
/// can still remain set on the synthesized character itself.
#[must_use]
pub const fn plan_character_modifiers(modifier_state: u8) -> CharacterModifierPlan {
    let shift_pressed = modifier_state & SHIFT_BIT != 0;
    let left_ctrl_pressed = modifier_state & CTRL_BIT != 0;
    let right_alt_pressed = modifier_state & ALT_BIT != 0;
    let alt_gr = modifier_state & (CTRL_BIT | ALT_BIT) == (CTRL_BIT | ALT_BIT);

    let envelope = if alt_gr {
        ModifierEnvelope::AltGr
    } else if shift_pressed {
        ModifierEnvelope::Shift
    } else {
        ModifierEnvelope::None
    };

    CharacterModifierPlan {
        shift_pressed,
        left_ctrl_pressed,
        right_alt_pressed,
        envelope,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn no_modifier_produces_no_envelope() {
        assert_eq!(
            plan_character_modifiers(0),
            CharacterModifierPlan {
                shift_pressed: false,
                left_ctrl_pressed: false,
                right_alt_pressed: false,
                envelope: ModifierEnvelope::None,
            }
        );
    }

    #[test]
    fn shift_gets_the_shift_envelope() {
        assert_eq!(
            plan_character_modifiers(SHIFT_BIT),
            CharacterModifierPlan {
                shift_pressed: true,
                left_ctrl_pressed: false,
                right_alt_pressed: false,
                envelope: ModifierEnvelope::Shift,
            }
        );
    }

    #[test]
    fn ctrl_or_alt_alone_only_marks_the_character() {
        assert_eq!(
            plan_character_modifiers(CTRL_BIT),
            CharacterModifierPlan {
                shift_pressed: false,
                left_ctrl_pressed: true,
                right_alt_pressed: false,
                envelope: ModifierEnvelope::None,
            }
        );
        assert_eq!(
            plan_character_modifiers(ALT_BIT),
            CharacterModifierPlan {
                shift_pressed: false,
                left_ctrl_pressed: false,
                right_alt_pressed: true,
                envelope: ModifierEnvelope::None,
            }
        );
    }

    #[test]
    fn ctrl_alt_uses_altgr_envelope() {
        assert_eq!(
            plan_character_modifiers(CTRL_BIT | ALT_BIT),
            CharacterModifierPlan {
                shift_pressed: false,
                left_ctrl_pressed: true,
                right_alt_pressed: true,
                envelope: ModifierEnvelope::AltGr,
            }
        );
    }

    #[test]
    fn altgr_precedes_shift_envelope_when_all_three_bits_are_set() {
        assert_eq!(
            plan_character_modifiers(SHIFT_BIT | CTRL_BIT | ALT_BIT),
            CharacterModifierPlan {
                shift_pressed: true,
                left_ctrl_pressed: true,
                right_alt_pressed: true,
                envelope: ModifierEnvelope::AltGr,
            }
        );
    }

    #[test]
    fn unrelated_high_bits_do_not_change_the_plan() {
        assert_eq!(plan_character_modifiers(0x80), plan_character_modifiers(0));
        assert_eq!(
            plan_character_modifiers(0x80 | SHIFT_BIT),
            plan_character_modifiers(SHIFT_BIT)
        );
    }
}
