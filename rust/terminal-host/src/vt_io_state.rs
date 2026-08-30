//! Deterministic lifecycle decisions from host `VtIo`.
//!
//! Win32 handle ownership, I/O threads, renderer creation, and console locking
//! remain outside this module. This type only preserves the state transitions
//! that decide whether initialization/start/shutdown/close work is required.

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum VtIoState {
    #[default]
    Uninitialized,
    Initialized,
    Starting,
    StartupFailed,
    Running,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum InitializeDecision {
    NotConpty,
    Initialized,
    AlreadyInitialized,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum StartDecision {
    NotNeeded,
    Begin,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FinishStartDecision {
    Running,
    StartupFailed,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CloseDecision {
    StartupAborted,
    SendCloseEvent,
    AlreadySent,
}

/// Platform-neutral lifecycle state extracted from `VtIo`.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct VtIoLifecycle {
    state: VtIoState,
    close_event_sent: bool,
}

impl VtIoLifecycle {
    #[must_use]
    pub const fn state(self) -> VtIoState {
        self.state
    }

    #[must_use]
    pub const fn is_using_vt(self) -> bool {
        !matches!(self.state, VtIoState::Uninitialized)
    }

    /// Mirrors `VtIo::Initialize` and `_Initialize` state decisions.
    pub fn initialize(&mut self, in_conpty_mode: bool) -> InitializeDecision {
        if !in_conpty_mode {
            return InitializeDecision::NotConpty;
        }
        if !matches!(self.state, VtIoState::Uninitialized) {
            return InitializeDecision::AlreadyInitialized;
        }

        self.state = VtIoState::Initialized;
        InitializeDecision::Initialized
    }

    /// Mirrors the entry condition of `VtIo::StartIfNeeded`.
    pub fn begin_start(&mut self) -> StartDecision {
        if !matches!(self.state, VtIoState::Initialized) {
            return StartDecision::NotNeeded;
        }

        self.state = VtIoState::Starting;
        StartDecision::Begin
    }

    /// Mirrors the final state check in `VtIo::StartIfNeeded` after external
    /// startup work and any temporary console-lock suspension have completed.
    pub fn finish_start(&mut self) -> FinishStartDecision {
        if matches!(self.state, VtIoState::Starting) {
            self.state = VtIoState::Running;
            FinishStartDecision::Running
        } else {
            FinishStartDecision::StartupFailed
        }
    }

    /// Mirrors the deterministic state portion of `VtIo::SendCloseEvent`.
    pub fn request_close(&mut self) -> CloseDecision {
        if matches!(self.state, VtIoState::Starting) {
            self.state = VtIoState::StartupFailed;
            return CloseDecision::StartupAborted;
        }

        if self.close_event_sent {
            CloseDecision::AlreadySent
        } else {
            self.close_event_sent = true;
            CloseDecision::SendCloseEvent
        }
    }

    /// `VtIo::Shutdown` emits terminal reset sequences only while Running.
    #[must_use]
    pub const fn should_emit_shutdown_sequences(self) -> bool {
        matches!(self.state, VtIoState::Running)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn non_conpty_initialization_is_a_noop() {
        let mut lifecycle = VtIoLifecycle::default();
        assert_eq!(lifecycle.initialize(false), InitializeDecision::NotConpty);
        assert_eq!(lifecycle.state(), VtIoState::Uninitialized);
        assert!(!lifecycle.is_using_vt());
    }

    #[test]
    fn initialization_is_single_assignment() {
        let mut lifecycle = VtIoLifecycle::default();
        assert_eq!(lifecycle.initialize(true), InitializeDecision::Initialized);
        assert_eq!(lifecycle.state(), VtIoState::Initialized);
        assert!(lifecycle.is_using_vt());
        assert_eq!(
            lifecycle.initialize(true),
            InitializeDecision::AlreadyInitialized
        );
    }

    #[test]
    fn normal_start_reaches_running() {
        let mut lifecycle = VtIoLifecycle::default();
        lifecycle.initialize(true);
        assert_eq!(lifecycle.begin_start(), StartDecision::Begin);
        assert_eq!(lifecycle.state(), VtIoState::Starting);
        assert_eq!(lifecycle.finish_start(), FinishStartDecision::Running);
        assert_eq!(lifecycle.state(), VtIoState::Running);
        assert!(lifecycle.should_emit_shutdown_sequences());
    }

    #[test]
    fn start_is_not_reentered_from_other_states() {
        let mut lifecycle = VtIoLifecycle::default();
        assert_eq!(lifecycle.begin_start(), StartDecision::NotNeeded);
        lifecycle.initialize(true);
        lifecycle.begin_start();
        lifecycle.finish_start();
        assert_eq!(lifecycle.begin_start(), StartDecision::NotNeeded);
    }

    #[test]
    fn close_during_start_marks_startup_failed_without_close_event() {
        let mut lifecycle = VtIoLifecycle::default();
        lifecycle.initialize(true);
        lifecycle.begin_start();

        assert_eq!(lifecycle.request_close(), CloseDecision::StartupAborted);
        assert_eq!(lifecycle.state(), VtIoState::StartupFailed);
        assert_eq!(lifecycle.finish_start(), FinishStartDecision::StartupFailed);
        assert!(!lifecycle.should_emit_shutdown_sequences());
    }

    #[test]
    fn close_event_is_deduplicated_after_startup() {
        let mut lifecycle = VtIoLifecycle::default();
        lifecycle.initialize(true);
        lifecycle.begin_start();
        lifecycle.finish_start();

        assert_eq!(lifecycle.request_close(), CloseDecision::SendCloseEvent);
        assert_eq!(lifecycle.request_close(), CloseDecision::AlreadySent);
        assert_eq!(lifecycle.state(), VtIoState::Running);
    }
}
