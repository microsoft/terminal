#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct TitleState {
    last_frame_title: String,
    title_changed: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TitleUpdate {
    Unchanged,
    UpdateRequired,
}

impl TitleState {
    #[must_use]
    pub fn last_frame_title(&self) -> &str {
        &self.last_frame_title
    }

    #[must_use]
    pub const fn title_changed(&self) -> bool {
        self.title_changed
    }

    pub fn invalidate(&mut self, proposed_title: &str) {
        if proposed_title != self.last_frame_title {
            self.title_changed = true;
        }
    }

    #[must_use]
    pub fn plan_update(&self, new_title: &str) -> TitleUpdate {
        if new_title == self.last_frame_title {
            TitleUpdate::Unchanged
        } else {
            TitleUpdate::UpdateRequired
        }
    }

    pub fn commit_update(&mut self, new_title: &str) -> TitleUpdate {
        let decision = self.plan_update(new_title);
        if decision == TitleUpdate::UpdateRequired {
            self.last_frame_title.clear();
            self.last_frame_title.push_str(new_title);
            self.title_changed = false;
        }
        decision
    }
}

#[cfg(test)]
mod tests {
    use super::{TitleState, TitleUpdate};

    #[test]
    fn invalidation_marks_only_changed_titles() {
        let mut state = TitleState::default();
        state.invalidate("");
        assert!(!state.title_changed());

        state.invalidate("Windows Terminal");
        assert!(state.title_changed());
    }

    #[test]
    fn update_is_skipped_for_the_current_title() {
        let mut state = TitleState::default();
        assert_eq!(state.commit_update("A"), TitleUpdate::UpdateRequired);
        assert_eq!(state.last_frame_title(), "A");

        state.invalidate("A");
        assert!(!state.title_changed());
        assert_eq!(state.commit_update("A"), TitleUpdate::Unchanged);
    }

    #[test]
    fn committed_update_clears_pending_invalidation() {
        let mut state = TitleState::default();
        state.invalidate("B");
        assert!(state.title_changed());

        assert_eq!(state.commit_update("B"), TitleUpdate::UpdateRequired);
        assert_eq!(state.last_frame_title(), "B");
        assert!(!state.title_changed());
    }

    #[test]
    fn failed_backend_update_can_leave_state_uncommitted() {
        let mut state = TitleState::default();
        state.invalidate("B");
        assert_eq!(state.plan_update("B"), TitleUpdate::UpdateRequired);

        assert_eq!(state.last_frame_title(), "");
        assert!(state.title_changed());
    }
}
