#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RenderMode {
    IndexedDistinguishableColors,
    AlwaysDistinguishableColors,
    IntenseIsBold,
    IntenseIsBright,
    ScreenReversed,
    SynchronizedOutput,
}

impl RenderMode {
    const fn mask(self) -> u8 {
        match self {
            Self::IndexedDistinguishableColors => 1 << 0,
            Self::AlwaysDistinguishableColors => 1 << 1,
            Self::IntenseIsBold => 1 << 2,
            Self::IntenseIsBright => 1 << 3,
            Self::ScreenReversed => 1 << 4,
            Self::SynchronizedOutput => 1 << 5,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RenderSettingsPolicy {
    modes: u8,
    blink_should_be_faint: bool,
}

impl Default for RenderSettingsPolicy {
    fn default() -> Self {
        Self {
            modes: RenderMode::IntenseIsBright.mask(),
            blink_should_be_faint: false,
        }
    }
}

impl RenderSettingsPolicy {
    pub fn set_mode(&mut self, mode: RenderMode, enabled: bool) {
        if enabled {
            self.modes |= mode.mask();
        } else {
            self.modes &= !mode.mask();
        }
    }

    #[must_use]
    pub const fn mode(self, mode: RenderMode) -> bool {
        self.modes & mode.mask() != 0
    }

    pub fn restore_programmable_defaults(&mut self) {
        self.set_mode(RenderMode::ScreenReversed, false);
        self.set_mode(RenderMode::SynchronizedOutput, false);
    }

    pub const fn toggle_blink_rendition(&mut self) {
        self.blink_should_be_faint = !self.blink_should_be_faint;
    }

    #[must_use]
    pub const fn blink_should_be_faint(self) -> bool {
        self.blink_should_be_faint
    }
}

#[cfg(test)]
mod tests {
    use super::{RenderMode, RenderSettingsPolicy};

    #[test]
    fn default_mode_matches_cpp_initial_state() {
        let settings = RenderSettingsPolicy::default();
        assert!(settings.mode(RenderMode::IntenseIsBright));
        assert!(!settings.mode(RenderMode::ScreenReversed));
        assert!(!settings.mode(RenderMode::SynchronizedOutput));
    }

    #[test]
    fn modes_can_be_changed_independently() {
        let mut settings = RenderSettingsPolicy::default();
        settings.set_mode(RenderMode::IntenseIsBold, true);
        settings.set_mode(RenderMode::IntenseIsBright, false);

        assert!(settings.mode(RenderMode::IntenseIsBold));
        assert!(!settings.mode(RenderMode::IntenseIsBright));
    }

    #[test]
    fn hard_reset_only_clears_programmable_modes() {
        let mut settings = RenderSettingsPolicy::default();
        settings.set_mode(RenderMode::AlwaysDistinguishableColors, true);
        settings.set_mode(RenderMode::ScreenReversed, true);
        settings.set_mode(RenderMode::SynchronizedOutput, true);

        settings.restore_programmable_defaults();

        assert!(settings.mode(RenderMode::AlwaysDistinguishableColors));
        assert!(settings.mode(RenderMode::IntenseIsBright));
        assert!(!settings.mode(RenderMode::ScreenReversed));
        assert!(!settings.mode(RenderMode::SynchronizedOutput));
    }

    #[test]
    fn blink_rendition_toggles_faint_state() {
        let mut settings = RenderSettingsPolicy::default();
        assert!(!settings.blink_should_be_faint());
        settings.toggle_blink_rendition();
        assert!(settings.blink_should_be_faint());
        settings.toggle_blink_rendition();
        assert!(!settings.blink_should_be_faint());
    }
}
