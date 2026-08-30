//! Safe control plane for Windows Terminal VT page management.
//!
//! The C++ `PageManager` combines deterministic page-number/lifecycle rules
//! with concrete `TextBuffer` row copies and renderer callbacks. R03e keeps the
//! deterministic rules here and emits typed [`PageEvent`] values for the
//! `TextBuffer`/renderer work that later migration slices will consume.

use crate::adapt_dispatch::{PageGeometry, Point};

pub const MAX_PAGES: i32 = 6;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PageSize {
    pub width: i32,
    pub height: i32,
}

impl PageSize {
    #[must_use]
    pub const fn new(width: i32, height: i32) -> Self {
        Self {
            width: if width < 1 { 1 } else { width },
            height: if height < 1 { 1 } else { height },
        }
    }
}

impl From<PageGeometry> for PageSize {
    fn from(value: PageGeometry) -> Self {
        Self::new(value.width, value.height)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PageBufferRef {
    Visible,
    Background(i32),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PageEvent {
    CreateBackgroundBuffer {
        page: i32,
        size: PageSize,
    },
    ResizeBackgroundBuffer {
        page: i32,
        old_size: PageSize,
        new_size: PageSize,
    },
    SaveVisibleRows {
        page: i32,
        visible_top: i32,
        size: PageSize,
    },
    LoadVisibleRows {
        page: i32,
        visible_top: i32,
        size: PageSize,
    },
    CopyProperties {
        from: PageBufferRef,
        to: PageBufferRef,
        old_top: i32,
        new_top: i32,
    },
    SetVisibleCursorVisible(bool),
    RedrawAll,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PageTransition {
    pub old_active_page: i32,
    pub new_active_page: i32,
    pub old_visible_page: i32,
    pub new_visible_page: i32,
    pub old_active_top: i32,
    pub new_active_top: i32,
}

impl PageTransition {
    #[must_use]
    pub fn adjust_point(self, point: Point) -> Point {
        Point {
            x: point.x,
            y: point
                .y
                .saturating_sub(self.old_active_top)
                .saturating_add(self.new_active_top),
        }
    }

    #[must_use]
    pub const fn active_changed(self) -> bool {
        self.old_active_page != self.new_active_page
    }

    #[must_use]
    pub const fn visible_changed(self) -> bool {
        self.old_visible_page != self.new_visible_page
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PageManager {
    visible_geometry: PageGeometry,
    active_page_number: i32,
    visible_page_number: i32,
    main_buffer: bool,
    backing_sizes: [Option<PageSize>; MAX_PAGES as usize],
    events: Vec<PageEvent>,
}

impl PageManager {
    #[must_use]
    pub fn new(visible_geometry: PageGeometry) -> Self {
        Self {
            visible_geometry,
            active_page_number: 1,
            visible_page_number: 1,
            main_buffer: true,
            backing_sizes: [None; MAX_PAGES as usize],
            events: Vec::new(),
        }
    }

    pub fn reset(&mut self) {
        self.active_page_number = 1;
        self.visible_page_number = 1;
        self.backing_sizes = [None; MAX_PAGES as usize];
        self.events.clear();
    }

    #[must_use]
    pub const fn active_page_number(&self) -> i32 {
        if self.main_buffer {
            self.active_page_number
        } else {
            1
        }
    }

    #[must_use]
    pub const fn visible_page_number(&self) -> i32 {
        if self.main_buffer {
            self.visible_page_number
        } else {
            1
        }
    }

    #[must_use]
    pub const fn is_main_buffer(&self) -> bool {
        self.main_buffer
    }

    pub const fn set_main_buffer(&mut self, main_buffer: bool) {
        self.main_buffer = main_buffer;
    }

    #[must_use]
    pub const fn visible_geometry(&self) -> PageGeometry {
        self.visible_geometry
    }

    pub fn set_visible_geometry(&mut self, geometry: PageGeometry) {
        self.visible_geometry = geometry;
    }

    #[must_use]
    pub const fn active_geometry(&self) -> PageGeometry {
        if !self.main_buffer || self.active_page_number == self.visible_page_number {
            self.visible_geometry
        } else {
            PageGeometry::new(0, self.visible_geometry.width, self.visible_geometry.height)
        }
    }

    #[must_use]
    pub fn pending_events(&self) -> &[PageEvent] {
        &self.events
    }

    pub fn take_events(&mut self) -> Vec<PageEvent> {
        std::mem::take(&mut self.events)
    }

    pub fn move_to(&mut self, page_number: i32, make_visible: bool) -> PageTransition {
        let old_active_page = self.active_page_number;
        let old_visible_page = self.visible_page_number;
        let old_active_top = self.top_for(old_active_page, old_visible_page);

        if !self.main_buffer {
            return PageTransition {
                old_active_page: 1,
                new_active_page: 1,
                old_visible_page: 1,
                new_visible_page: 1,
                old_active_top: self.visible_geometry.top,
                new_active_top: self.visible_geometry.top,
            };
        }

        let new_page_number = page_number.clamp(1, MAX_PAGES);
        let was_visible = old_active_page == old_visible_page;
        let size = PageSize::from(self.visible_geometry);
        let mut redraw_required = false;

        if make_visible && self.visible_page_number != new_page_number {
            self.ensure_background(new_page_number, size);
            self.ensure_background(self.visible_page_number, size);
            self.events.push(PageEvent::SaveVisibleRows {
                page: self.visible_page_number,
                visible_top: self.visible_geometry.top,
                size,
            });
            self.events.push(PageEvent::LoadVisibleRows {
                page: new_page_number,
                visible_top: self.visible_geometry.top,
                size,
            });
            self.visible_page_number = new_page_number;
            redraw_required = true;
        }

        let is_visible = new_page_number == self.visible_page_number;
        if !was_visible || !is_visible {
            let old_buffer = if was_visible {
                PageBufferRef::Visible
            } else {
                self.ensure_background(old_active_page, size);
                PageBufferRef::Background(old_active_page)
            };
            let new_buffer = if is_visible {
                PageBufferRef::Visible
            } else {
                self.ensure_background(new_page_number, size);
                PageBufferRef::Background(new_page_number)
            };

            if old_buffer != new_buffer {
                let old_top = if was_visible {
                    self.visible_geometry.top
                } else {
                    0
                };
                let new_top = if is_visible {
                    self.visible_geometry.top
                } else {
                    0
                };
                self.events.push(PageEvent::CopyProperties {
                    from: old_buffer,
                    to: new_buffer,
                    old_top,
                    new_top,
                });
            }

            if was_visible && !is_visible {
                self.events.push(PageEvent::SetVisibleCursorVisible(false));
            }
        }

        self.active_page_number = new_page_number;
        if redraw_required {
            self.events.push(PageEvent::RedrawAll);
        }
        let new_active_top = self.top_for(self.active_page_number, self.visible_page_number);

        PageTransition {
            old_active_page,
            new_active_page: self.active_page_number,
            old_visible_page,
            new_visible_page: self.visible_page_number,
            old_active_top,
            new_active_top,
        }
    }

    pub fn move_relative(&mut self, page_count: i32, make_visible: bool) -> PageTransition {
        self.move_to(
            self.active_page_number.saturating_add(page_count),
            make_visible,
        )
    }

    pub fn make_active_page_visible(&mut self) -> Option<PageTransition> {
        (self.main_buffer && self.active_page_number != self.visible_page_number)
            .then(|| self.move_to(self.active_page_number, true))
    }

    fn top_for(&self, page: i32, visible_page: i32) -> i32 {
        if page == visible_page {
            self.visible_geometry.top
        } else {
            0
        }
    }

    fn ensure_background(&mut self, page: i32, size: PageSize) -> PageBufferRef {
        let index = usize::try_from(page.clamp(1, MAX_PAGES) - 1)
            .expect("clamped page index is nonnegative");
        match self.backing_sizes[index] {
            None => {
                self.backing_sizes[index] = Some(size);
                self.events
                    .push(PageEvent::CreateBackgroundBuffer { page, size });
            }
            Some(old_size) if old_size != size => {
                self.backing_sizes[index] = Some(size);
                self.events.push(PageEvent::ResizeBackgroundBuffer {
                    page,
                    old_size,
                    new_size: size,
                });
            }
            Some(_) => {}
        }
        PageBufferRef::Background(page)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn geometry() -> PageGeometry {
        PageGeometry::new(20, 80, 24)
    }

    #[test]
    fn defaults_and_page_clamping_match_microsoft() {
        let mut pages = PageManager::new(geometry());
        assert_eq!(pages.active_page_number(), 1);
        assert_eq!(pages.visible_page_number(), 1);

        let transition = pages.move_to(9_999, false);
        assert_eq!(pages.active_page_number(), 6);
        assert_eq!(pages.visible_page_number(), 1);
        assert_eq!(
            transition.adjust_point(Point { x: 7, y: 31 }),
            Point { x: 7, y: 11 }
        );

        let transition = pages.move_to(-9_999, false);
        assert_eq!(pages.active_page_number(), 1);
        assert_eq!(
            transition.adjust_point(Point { x: 7, y: 11 }),
            Point { x: 7, y: 31 }
        );
    }

    #[test]
    fn coupled_move_swaps_visible_rows_and_requests_redraw() {
        let mut pages = PageManager::new(geometry());
        let transition = pages.move_to(3, true);

        assert_eq!(pages.active_page_number(), 3);
        assert_eq!(pages.visible_page_number(), 3);
        assert!(transition.active_changed());
        assert!(transition.visible_changed());
        assert_eq!(transition.old_active_top, 20);
        assert_eq!(transition.new_active_top, 20);
        assert_eq!(
            pages.take_events(),
            vec![
                PageEvent::CreateBackgroundBuffer {
                    page: 3,
                    size: PageSize::new(80, 24),
                },
                PageEvent::CreateBackgroundBuffer {
                    page: 1,
                    size: PageSize::new(80, 24),
                },
                PageEvent::SaveVisibleRows {
                    page: 1,
                    visible_top: 20,
                    size: PageSize::new(80, 24),
                },
                PageEvent::LoadVisibleRows {
                    page: 3,
                    visible_top: 20,
                    size: PageSize::new(80, 24),
                },
                PageEvent::RedrawAll,
            ]
        );
    }

    #[test]
    fn uncoupled_move_copies_properties_and_hides_visible_cursor() {
        let mut pages = PageManager::new(geometry());
        let transition = pages.move_to(4, false);

        assert_eq!(pages.active_page_number(), 4);
        assert_eq!(pages.visible_page_number(), 1);
        assert_eq!(transition.old_active_top, 20);
        assert_eq!(transition.new_active_top, 0);
        assert_eq!(
            pages.take_events(),
            vec![
                PageEvent::CreateBackgroundBuffer {
                    page: 4,
                    size: PageSize::new(80, 24),
                },
                PageEvent::CopyProperties {
                    from: PageBufferRef::Visible,
                    to: PageBufferRef::Background(4),
                    old_top: 20,
                    new_top: 0,
                },
                PageEvent::SetVisibleCursorVisible(false),
            ]
        );
    }

    #[test]
    fn recoupling_makes_active_page_visible_and_preserves_active_coordinates() {
        let mut pages = PageManager::new(geometry());
        let first = pages.move_to(4, false);
        let background_cursor = first.adjust_point(Point { x: 30, y: 35 });
        pages.take_events();

        let transition = pages
            .make_active_page_visible()
            .expect("background active page should become visible");
        assert_eq!(pages.active_page_number(), 4);
        assert_eq!(pages.visible_page_number(), 4);
        assert_eq!(
            transition.adjust_point(background_cursor),
            Point { x: 30, y: 35 }
        );
        assert_eq!(
            pages.take_events(),
            vec![
                PageEvent::CreateBackgroundBuffer {
                    page: 1,
                    size: PageSize::new(80, 24),
                },
                PageEvent::SaveVisibleRows {
                    page: 1,
                    visible_top: 20,
                    size: PageSize::new(80, 24),
                },
                PageEvent::LoadVisibleRows {
                    page: 4,
                    visible_top: 20,
                    size: PageSize::new(80, 24),
                },
                PageEvent::CopyProperties {
                    from: PageBufferRef::Background(4),
                    to: PageBufferRef::Visible,
                    old_top: 0,
                    new_top: 20,
                },
                PageEvent::RedrawAll,
            ]
        );
    }

    #[test]
    fn relative_moves_use_saturating_arithmetic_and_clamp_to_six_pages() {
        let mut pages = PageManager::new(geometry());
        pages.move_relative(i32::MAX, false);
        assert_eq!(pages.active_page_number(), 6);
        pages.move_relative(i32::MIN, false);
        assert_eq!(pages.active_page_number(), 1);
    }

    #[test]
    fn background_buffers_resize_lazily_when_visible_dimensions_change() {
        let mut pages = PageManager::new(geometry());
        pages.move_to(2, false);
        pages.take_events();
        pages.move_to(1, false);
        pages.take_events();

        pages.set_visible_geometry(PageGeometry::new(40, 100, 30));
        pages.move_to(2, false);
        assert!(
            pages
                .pending_events()
                .contains(&PageEvent::ResizeBackgroundBuffer {
                    page: 2,
                    old_size: PageSize::new(80, 24),
                    new_size: PageSize::new(100, 30),
                })
        );
        assert_eq!(pages.active_geometry(), PageGeometry::new(0, 100, 30));
    }

    #[test]
    fn non_main_buffer_ignores_paging_and_reports_page_one() {
        let mut pages = PageManager::new(geometry());
        pages.move_to(4, false);
        pages.take_events();
        pages.set_main_buffer(false);

        let transition = pages.move_to(6, true);
        assert_eq!(pages.active_page_number(), 1);
        assert_eq!(pages.visible_page_number(), 1);
        assert!(!transition.active_changed());
        assert!(!transition.visible_changed());
        assert!(pages.pending_events().is_empty());
        assert_eq!(pages.active_geometry(), geometry());
    }

    #[test]
    fn reset_releases_background_page_state() {
        let mut pages = PageManager::new(geometry());
        pages.move_to(5, false);
        assert!(!pages.pending_events().is_empty());

        pages.reset();
        assert_eq!(pages.active_page_number(), 1);
        assert_eq!(pages.visible_page_number(), 1);
        assert!(pages.pending_events().is_empty());

        pages.move_to(5, false);
        assert!(matches!(
            pages.pending_events().first(),
            Some(PageEvent::CreateBackgroundBuffer { page: 5, .. })
        ));
    }
}
