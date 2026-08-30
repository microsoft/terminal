//! Safe equivalent of `TerminalCore` `ControlKeyStates`.
//!
//! The bit values intentionally remain compatible with the NT console
//! `KEY_EVENT_RECORD` control-key-state flags. The two Windows-key bits are the
//! same Terminal-specific extension used by the C++ implementation.

use core::ops::{BitAnd, BitOr, BitOrAssign};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct ControlKeyStates(u32);

impl ControlKeyStates {
    pub const RIGHT_ALT_PRESSED: Self = Self(0x0001);
    pub const LEFT_ALT_PRESSED: Self = Self(0x0002);
    pub const RIGHT_CTRL_PRESSED: Self = Self(0x0004);
    pub const LEFT_CTRL_PRESSED: Self = Self(0x0008);
    pub const SHIFT_PRESSED: Self = Self(0x0010);
    pub const NUMLOCK_ON: Self = Self(0x0020);
    pub const SCROLLLOCK_ON: Self = Self(0x0040);
    pub const CAPSLOCK_ON: Self = Self(0x0080);
    pub const ENHANCED_KEY: Self = Self(0x0100);
    pub const RIGHT_WIN_PRESSED: Self = Self(0x0200);
    pub const LEFT_WIN_PRESSED: Self = Self(0x0400);

    #[must_use]
    pub const fn from_bits(value: u32) -> Self {
        Self(value)
    }

    #[must_use]
    pub const fn bits(self) -> u32 {
        self.0
    }

    #[must_use]
    pub const fn is_shift_pressed(self) -> bool {
        self.is_any_flag_set(Self::SHIFT_PRESSED)
    }

    #[must_use]
    pub const fn is_alt_pressed(self) -> bool {
        self.is_any_flag_set(Self::RIGHT_ALT_PRESSED.or(Self::LEFT_ALT_PRESSED))
    }

    #[must_use]
    pub const fn is_ctrl_pressed(self) -> bool {
        self.is_any_flag_set(Self::RIGHT_CTRL_PRESSED.or(Self::LEFT_CTRL_PRESSED))
    }

    #[must_use]
    pub const fn is_win_pressed(self) -> bool {
        self.is_any_flag_set(Self::RIGHT_WIN_PRESSED.or(Self::LEFT_WIN_PRESSED))
    }

    #[must_use]
    pub const fn is_alt_gr_pressed(self) -> bool {
        self.are_all_flags_set(Self::RIGHT_ALT_PRESSED.or(Self::LEFT_CTRL_PRESSED))
    }

    #[must_use]
    pub const fn is_modifier_pressed(self) -> bool {
        let alt = Self::RIGHT_ALT_PRESSED.or(Self::LEFT_ALT_PRESSED);
        let ctrl = Self::RIGHT_CTRL_PRESSED.or(Self::LEFT_CTRL_PRESSED);
        self.is_any_flag_set(alt.or(ctrl).or(Self::SHIFT_PRESSED))
    }

    #[must_use]
    pub const fn contains_all(self, mask: Self) -> bool {
        self.are_all_flags_set(mask)
    }

    #[must_use]
    pub const fn intersects(self, mask: Self) -> bool {
        self.is_any_flag_set(mask)
    }

    const fn or(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }

    const fn are_all_flags_set(self, mask: Self) -> bool {
        (self.0 & mask.0) == mask.0
    }

    const fn is_any_flag_set(self, mask: Self) -> bool {
        (self.0 & mask.0) != 0
    }
}

impl From<u32> for ControlKeyStates {
    fn from(value: u32) -> Self {
        Self::from_bits(value)
    }
}

impl From<ControlKeyStates> for u32 {
    fn from(value: ControlKeyStates) -> Self {
        value.bits()
    }
}

impl BitOr for ControlKeyStates {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        self.or(rhs)
    }
}

impl BitOrAssign for ControlKeyStates {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl BitAnd for ControlKeyStates {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bit_values_match_wincon_and_terminal_extensions() {
        assert_eq!(ControlKeyStates::RIGHT_ALT_PRESSED.bits(), 0x0001);
        assert_eq!(ControlKeyStates::LEFT_ALT_PRESSED.bits(), 0x0002);
        assert_eq!(ControlKeyStates::RIGHT_CTRL_PRESSED.bits(), 0x0004);
        assert_eq!(ControlKeyStates::LEFT_CTRL_PRESSED.bits(), 0x0008);
        assert_eq!(ControlKeyStates::SHIFT_PRESSED.bits(), 0x0010);
        assert_eq!(ControlKeyStates::NUMLOCK_ON.bits(), 0x0020);
        assert_eq!(ControlKeyStates::SCROLLLOCK_ON.bits(), 0x0040);
        assert_eq!(ControlKeyStates::CAPSLOCK_ON.bits(), 0x0080);
        assert_eq!(ControlKeyStates::ENHANCED_KEY.bits(), 0x0100);
        assert_eq!(ControlKeyStates::RIGHT_WIN_PRESSED.bits(), 0x0200);
        assert_eq!(ControlKeyStates::LEFT_WIN_PRESSED.bits(), 0x0400);
    }

    #[test]
    fn modifier_queries_match_cpp_semantics() {
        let states = ControlKeyStates::RIGHT_ALT_PRESSED
            | ControlKeyStates::LEFT_CTRL_PRESSED
            | ControlKeyStates::SHIFT_PRESSED;

        assert!(states.is_alt_pressed());
        assert!(states.is_ctrl_pressed());
        assert!(states.is_shift_pressed());
        assert!(states.is_alt_gr_pressed());
        assert!(states.is_modifier_pressed());
        assert!(!states.is_win_pressed());
    }

    #[test]
    fn alt_gr_requires_exact_required_pair_but_allows_extra_flags() {
        let partial = ControlKeyStates::RIGHT_ALT_PRESSED;
        assert!(!partial.is_alt_gr_pressed());

        let full = partial | ControlKeyStates::LEFT_CTRL_PRESSED | ControlKeyStates::CAPSLOCK_ON;
        assert!(full.is_alt_gr_pressed());
    }

    #[test]
    fn lock_and_enhanced_bits_are_not_modifiers() {
        let states = ControlKeyStates::NUMLOCK_ON
            | ControlKeyStates::SCROLLLOCK_ON
            | ControlKeyStates::CAPSLOCK_ON
            | ControlKeyStates::ENHANCED_KEY;

        assert!(!states.is_modifier_pressed());
        assert!(!states.is_alt_pressed());
        assert!(!states.is_ctrl_pressed());
        assert!(!states.is_shift_pressed());
        assert!(!states.is_win_pressed());
    }

    #[test]
    fn bitwise_operations_preserve_unknown_bits() {
        let unknown = ControlKeyStates::from_bits(0x8000_0000);
        let combined = unknown | ControlKeyStates::SHIFT_PRESSED;

        assert_eq!(combined.bits(), 0x8000_0010);
        assert_eq!(combined & unknown, unknown);
        assert!(combined.contains_all(ControlKeyStates::SHIFT_PRESSED));
        assert!(combined.intersects(unknown));
    }
}
