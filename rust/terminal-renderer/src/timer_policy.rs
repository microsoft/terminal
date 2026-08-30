pub type TimerRepr = u64;

pub const TIMER_REPR_MAX: TimerRepr = TimerRepr::MAX;

#[must_use]
pub const fn saturating_timer_add(a: TimerRepr, b: TimerRepr) -> TimerRepr {
    a.saturating_add(b)
}

#[must_use]
pub const fn saturating_timer_sub(a: TimerRepr, b: TimerRepr) -> TimerRepr {
    a.saturating_sub(b)
}

#[must_use]
pub fn timer_to_millis(ticks_100ns: TimerRepr) -> u32 {
    let millis = ticks_100ns / 10_000;
    u32::try_from(millis).unwrap_or(u32::MAX)
}

#[must_use]
pub const fn reschedule_repeating_timer(
    scheduled: TimerRepr,
    interval: TimerRepr,
    now: TimerRepr,
) -> TimerRepr {
    let next = saturating_timer_add(scheduled, interval);
    if next <= now {
        saturating_timer_add(now, interval)
    } else {
        next
    }
}

#[cfg(test)]
mod tests {
    use super::{
        TIMER_REPR_MAX, reschedule_repeating_timer, saturating_timer_add, saturating_timer_sub,
        timer_to_millis,
    };

    #[test]
    fn timer_add_and_subtract_saturate_at_the_integer_bounds() {
        assert_eq!(saturating_timer_add(TIMER_REPR_MAX - 1, 2), TIMER_REPR_MAX);
        assert_eq!(saturating_timer_sub(1, 2), 0);
        assert_eq!(saturating_timer_add(10, 20), 30);
        assert_eq!(saturating_timer_sub(30, 20), 10);
    }

    #[test]
    fn timer_conversion_truncates_to_milliseconds_and_clamps_to_dword() {
        assert_eq!(timer_to_millis(9_999), 0);
        assert_eq!(timer_to_millis(10_000), 1);
        assert_eq!(timer_to_millis(TIMER_REPR_MAX), u32::MAX);
    }

    #[test]
    fn repeating_timer_preserves_original_schedule_without_drift() {
        assert_eq!(reschedule_repeating_timer(100, 20, 105), 120);
    }

    #[test]
    fn repeating_timer_moves_forward_from_now_if_schedule_is_already_past() {
        assert_eq!(reschedule_repeating_timer(100, 20, 145), 165);
    }

    #[test]
    fn repeating_timer_reschedule_saturates() {
        assert_eq!(
            reschedule_repeating_timer(TIMER_REPR_MAX - 5, 10, TIMER_REPR_MAX - 1),
            TIMER_REPR_MAX
        );
    }
}
