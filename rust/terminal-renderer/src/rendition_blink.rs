pub const RENDITION_BLINK_INTERVAL_100NS: u64 = 10_000_000;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RenditionBlinkAction {
    Unchanged,
    StartRepeating { interval_100ns: u64 },
    Stop,
}

#[must_use]
pub const fn plan_rendition_blink(blink_used: bool, timer_running: bool) -> RenditionBlinkAction {
    match (blink_used, timer_running) {
        (true, false) => RenditionBlinkAction::StartRepeating {
            interval_100ns: RENDITION_BLINK_INTERVAL_100NS,
        },
        (false, true) => RenditionBlinkAction::Stop,
        _ => RenditionBlinkAction::Unchanged,
    }
}

#[cfg(test)]
mod tests {
    use super::{RENDITION_BLINK_INTERVAL_100NS, RenditionBlinkAction, plan_rendition_blink};

    #[test]
    fn starts_one_second_repeating_timer_when_blink_first_appears() {
        assert_eq!(
            plan_rendition_blink(true, false),
            RenditionBlinkAction::StartRepeating {
                interval_100ns: RENDITION_BLINK_INTERVAL_100NS,
            }
        );
    }

    #[test]
    fn stops_timer_when_blink_disappears() {
        assert_eq!(
            plan_rendition_blink(false, true),
            RenditionBlinkAction::Stop
        );
    }

    #[test]
    fn leaves_matching_timer_state_unchanged() {
        assert_eq!(
            plan_rendition_blink(true, true),
            RenditionBlinkAction::Unchanged
        );
        assert_eq!(
            plan_rendition_blink(false, false),
            RenditionBlinkAction::Unchanged
        );
    }
}
