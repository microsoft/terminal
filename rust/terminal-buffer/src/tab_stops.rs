//! Safe horizontal tab-stop state for the screen-buffer seam.
//!
//! Windows Terminal keeps tab stops as screen-buffer state: defaults occur every
//! eight columns, explicit stops are unique and ordered, forward/reverse tabbing
//! clamps at the buffer edges, and the main/alternate screen-buffer views share
//! the same tab-stop state. This module owns those deterministic semantics
//! without parser, renderer, or Win32 dependencies.

use std::cell::RefCell;
use std::collections::BTreeSet;
use std::rc::Rc;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TabStops {
    width: u16,
    stops: BTreeSet<u16>,
}

impl TabStops {
    #[must_use]
    pub fn new(width: u16) -> Self {
        let mut value = Self {
            width: width.max(1),
            stops: BTreeSet::new(),
        };
        value.reset_defaults();
        value
    }

    #[must_use]
    pub const fn width(&self) -> u16 {
        self.width
    }

    #[must_use]
    pub fn stops(&self) -> Vec<u16> {
        self.stops.iter().copied().collect()
    }

    pub fn reset_defaults(&mut self) {
        self.stops.clear();
        let mut column = 8_u16;
        while column < self.width {
            self.stops.insert(column);
            column = column.saturating_add(8);
        }
    }

    pub fn clear_all(&mut self) {
        self.stops.clear();
    }

    pub fn add(&mut self, column: u16) {
        if column < self.width {
            self.stops.insert(column);
        }
    }

    pub fn clear_at(&mut self, column: u16) {
        self.stops.remove(&column);
    }

    pub fn replace<I>(&mut self, columns: I)
    where
        I: IntoIterator<Item = u16>,
    {
        self.stops.clear();
        self.stops
            .extend(columns.into_iter().filter(|column| *column < self.width));
    }

    #[must_use]
    pub fn forward_from(&self, column: u16) -> u16 {
        self.stops
            .range(column.saturating_add(1)..)
            .next()
            .copied()
            .unwrap_or_else(|| self.width - 1)
    }

    #[must_use]
    pub fn reverse_from(&self, column: u16) -> u16 {
        self.stops.range(..column).next_back().copied().unwrap_or(0)
    }
}

/// Cloneable handle used by main and alternate screen-buffer views.
///
/// Cloning this value intentionally shares the tab-stop owner. Microsoft copies
/// the active buffer view but keeps tab-stop mutations visible after returning
/// from the alternate screen buffer.
#[derive(Debug, Clone)]
pub struct SharedTabStops {
    inner: Rc<RefCell<TabStops>>,
}

impl SharedTabStops {
    #[must_use]
    pub fn new(width: u16) -> Self {
        Self {
            inner: Rc::new(RefCell::new(TabStops::new(width))),
        }
    }

    #[must_use]
    pub fn stops(&self) -> Vec<u16> {
        self.inner.borrow().stops()
    }

    pub fn reset_defaults(&self) {
        self.inner.borrow_mut().reset_defaults();
    }

    pub fn clear_all(&self) {
        self.inner.borrow_mut().clear_all();
    }

    pub fn add(&self, column: u16) {
        self.inner.borrow_mut().add(column);
    }

    pub fn clear_at(&self, column: u16) {
        self.inner.borrow_mut().clear_at(column);
    }

    pub fn replace<I>(&self, columns: I)
    where
        I: IntoIterator<Item = u16>,
    {
        self.inner.borrow_mut().replace(columns);
    }

    #[must_use]
    pub fn forward_from(&self, column: u16) -> u16 {
        self.inner.borrow().forward_from(column)
    }

    #[must_use]
    pub fn reverse_from(&self, column: u16) -> u16 {
        self.inner.borrow().reverse_from(column)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_stops() -> [u16; 6] {
        [3, 5, 6, 10, 15, 17]
    }

    fn default_stops() -> Vec<u16> {
        vec![8, 16, 24, 32, 40, 48, 56, 64, 72]
    }

    #[test]
    fn microsoft_screen_buffer_reset_clear_tab_stops_contract() {
        let mut tabs = TabStops::new(80);
        assert_eq!(tabs.stops(), default_stops());

        tabs.clear_all();
        assert!(tabs.stops().is_empty());

        tabs.reset_defaults();
        assert_eq!(tabs.stops(), default_stops());

        tabs.clear_all();
        tabs.reset_defaults();
        assert_eq!(tabs.stops(), default_stops());

        tabs.clear_all();
        tabs.reset_defaults();
        assert_eq!(tabs.stops(), default_stops());
    }

    #[test]
    fn microsoft_screen_buffer_add_tab_stop_contract() {
        let mut tabs = TabStops::new(80);
        tabs.clear_all();
        assert!(tabs.stops().is_empty());

        tabs.add(12);
        assert_eq!(tabs.stops(), vec![12]);

        tabs.add(4);
        assert_eq!(tabs.stops(), vec![4, 12]);

        tabs.add(30);
        assert_eq!(tabs.stops(), vec![4, 12, 30]);

        tabs.add(24);
        assert_eq!(tabs.stops(), vec![4, 12, 24, 30]);

        tabs.add(24);
        assert_eq!(tabs.stops(), vec![4, 12, 24, 30]);
    }

    #[test]
    fn microsoft_screen_buffer_clear_tab_stop_contract() {
        let mut tabs = TabStops::new(80);
        tabs.clear_all();
        assert!(tabs.stops().is_empty());

        tabs.clear_at(0);
        assert!(tabs.stops().is_empty());

        tabs.add(0);
        tabs.clear_at(0);
        assert!(tabs.stops().is_empty());

        tabs.add(1);
        tabs.clear_at(2);
        assert_eq!(tabs.stops(), vec![1]);
        tabs.clear_at(0);
        assert_eq!(tabs.stops(), vec![1]);
        tabs.clear_all();

        tabs.replace([3, 5, 6, 10, 15, 17]);
        tabs.clear_at(3);
        assert_eq!(tabs.stops(), vec![5, 6, 10, 15, 17]);
        tabs.clear_all();

        tabs.replace([3, 5, 6, 10, 15, 17]);
        tabs.clear_at(5);
        assert_eq!(tabs.stops(), vec![3, 6, 10, 15, 17]);
        tabs.clear_all();

        tabs.replace([3, 5, 6, 10, 15, 17]);
        tabs.clear_at(17);
        assert_eq!(tabs.stops(), vec![3, 5, 6, 10, 15]);
        tabs.clear_all();

        tabs.replace([3, 5, 6, 10, 15, 17]);
        tabs.clear_at(0);
        assert_eq!(tabs.stops(), sample_stops());
    }

    #[test]
    fn microsoft_screen_buffer_get_forward_tab_contract() {
        let mut tabs = TabStops::new(80);
        tabs.replace(sample_stops());

        assert_eq!(tabs.forward_from(0), 3);
        assert_eq!(tabs.forward_from(6), 10);
        assert_eq!(tabs.forward_from(30), 79);
        assert_eq!(tabs.forward_from(79), 79);
    }

    #[test]
    fn microsoft_screen_buffer_get_reverse_tab_contract() {
        let mut tabs = TabStops::new(80);
        tabs.replace(sample_stops());

        assert_eq!(tabs.reverse_from(1), 0);
        assert_eq!(tabs.reverse_from(6), 5);
        assert_eq!(tabs.reverse_from(30), 17);
    }

    #[test]
    fn microsoft_screen_buffer_alt_buffer_tab_stops_contract() {
        let main = SharedTabStops::new(80);
        main.replace(sample_stops());
        assert_eq!(main.stops(), sample_stops());

        let alternate = main.clone();
        assert_eq!(alternate.stops(), sample_stops());

        alternate.replace([4, 8, 12, 16]);
        assert_eq!(alternate.stops(), vec![4, 8, 12, 16]);
        assert_eq!(main.stops(), vec![4, 8, 12, 16]);
    }
}
