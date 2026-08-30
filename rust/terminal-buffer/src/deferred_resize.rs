//! Portable coordination for main-buffer resizes requested while the alternate
//! screen buffer is active.
//!
//! Windows Terminal deliberately defers mutating the main buffer until the
//! alternate buffer is released. The active alternate buffer still adopts the
//! requested terminal dimensions immediately, including when `DECSET 1049` is
//! sent again and replaces the current alternate buffer.

use crate::alternate_buffer::ViewportSize;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeferredMainResizeState {
    main_buffer_size: ViewportSize,
    main_viewport: ViewportSize,
    alternate_size: Option<ViewportSize>,
    pending_main_resize: Option<ViewportSize>,
    alternate_generation: u64,
}

impl DeferredMainResizeState {
    #[must_use]
    pub fn new(main_buffer_size: ViewportSize, main_viewport: ViewportSize) -> Self {
        assert!(main_buffer_size.width >= main_viewport.width);
        assert!(main_buffer_size.height >= main_viewport.height);
        Self {
            main_buffer_size,
            main_viewport,
            alternate_size: None,
            pending_main_resize: None,
            alternate_generation: 0,
        }
    }

    /// Enters (or re-enters) the alternate screen buffer.
    ///
    /// Re-entry creates a new logical alternate generation but preserves the
    /// current alternate dimensions. This mirrors `DECSET 1049` replacing the
    /// active alternate while a main-buffer resize remains deferred.
    pub fn enter_alternate(&mut self) {
        let size = self.alternate_size.unwrap_or(self.main_viewport);
        self.alternate_size = Some(size);
        self.alternate_generation += 1;
    }

    /// Applies a terminal resize to the active buffer.
    ///
    /// While the alternate is active it is viewport-sized immediately, while
    /// the corresponding main-buffer mutation is retained as pending work.
    pub fn resize_terminal(&mut self, size: ViewportSize) {
        assert!(size.width > 0 && size.height > 0);
        if self.alternate_size.is_some() {
            self.alternate_size = Some(size);
            self.pending_main_resize = Some(size);
        } else {
            self.main_buffer_size = size;
            self.main_viewport = size;
        }
    }

    /// Leaves the alternate buffer and materializes the deferred main resize.
    pub fn leave_alternate(&mut self) {
        self.alternate_size = None;
        if let Some(size) = self.pending_main_resize.take() {
            self.main_buffer_size = size;
            self.main_viewport = size;
        }
    }

    #[must_use]
    pub const fn main_buffer_size(&self) -> ViewportSize {
        self.main_buffer_size
    }

    #[must_use]
    pub const fn main_viewport(&self) -> ViewportSize {
        self.main_viewport
    }

    #[must_use]
    pub const fn alternate_size(&self) -> Option<ViewportSize> {
        self.alternate_size
    }

    #[must_use]
    pub const fn pending_main_resize(&self) -> Option<ViewportSize> {
        self.pending_main_resize
    }

    #[must_use]
    pub const fn alternate_generation(&self) -> u64 {
        self.alternate_generation
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn microsoft_deferred_main_buffer_resize_case(reenter_alternate: bool) {
        let old_size = ViewportSize {
            width: 80,
            height: 100,
        };
        let old_view = ViewportSize {
            width: 80,
            height: 25,
        };
        assert_ne!(old_size, old_view);

        let mut state = DeferredMainResizeState::new(old_size, old_view);
        state.enter_alternate();
        assert_eq!(state.alternate_size(), Some(old_view));

        let expected_size = ViewportSize {
            width: 60,
            height: 24,
        };
        state.resize_terminal(expected_size);

        // The alternate buffer is always viewport-sized and updates now.
        assert_eq!(state.alternate_size(), Some(expected_size));
        // The main buffer/view stay untouched until the alternate is released.
        assert_eq!(state.main_buffer_size(), old_size);
        assert_eq!(state.main_viewport(), old_view);
        assert_eq!(state.pending_main_resize(), Some(expected_size));

        if reenter_alternate {
            let generation = state.alternate_generation();
            state.enter_alternate();
            assert_eq!(state.alternate_generation(), generation + 1);
            assert_eq!(state.alternate_size(), Some(expected_size));
            assert_eq!(state.pending_main_resize(), Some(expected_size));
        }

        state.leave_alternate();
        assert_eq!(state.alternate_size(), None);
        assert_eq!(state.pending_main_resize(), None);
        assert_ne!(state.main_viewport(), old_view);
        assert_ne!(state.main_buffer_size(), old_size);
        assert_eq!(state.main_viewport(), expected_size);
        assert_eq!(state.main_buffer_size(), expected_size);
    }

    #[test]
    fn microsoft_deferred_main_buffer_resize_contract() {
        microsoft_deferred_main_buffer_resize_case(false);
    }

    #[test]
    fn microsoft_deferred_main_buffer_resize_reentry_contract() {
        microsoft_deferred_main_buffer_resize_case(true);
    }
}
