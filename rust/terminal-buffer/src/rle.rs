//! Safe run-length encoded storage compatible with the observable semantics
//! needed from TIL's `basic_rle`/`small_rle` containers.
//!
//! The representation is deliberately simple: adjacent equal values are
//! canonicalized into one run, zero-length runs are never retained, and the
//! encoded length is tracked independently from the number of runs.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Run<T> {
    pub value: T,
    pub length: usize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rle<T> {
    runs: Vec<Run<T>>,
    len: usize,
}

impl<T> Default for Rle<T> {
    fn default() -> Self {
        Self {
            runs: Vec::new(),
            len: 0,
        }
    }
}

impl<T: Clone + Eq> Rle<T> {
    #[must_use]
    pub fn new(length: usize, value: T) -> Self {
        if length == 0 {
            return Self::default();
        }

        Self {
            runs: vec![Run { value, length }],
            len: length,
        }
    }

    #[must_use]
    pub const fn is_empty(&self) -> bool {
        self.len == 0
    }

    #[must_use]
    pub const fn len(&self) -> usize {
        self.len
    }

    #[must_use]
    pub fn runs(&self) -> &[Run<T>] {
        &self.runs
    }

    #[must_use]
    pub fn at(&self, position: usize) -> Option<&T> {
        if position >= self.len {
            return None;
        }

        let mut offset = 0usize;
        for run in &self.runs {
            let end = offset.saturating_add(run.length);
            if position < end {
                return Some(&run.value);
            }
            offset = end;
        }

        None
    }

    /// Replaces the half-open range `[begin, end)` with one value.
    ///
    /// Indices are clamped to the encoded length. Empty or reversed ranges are
    /// no-ops, matching the range behavior used by the row attribute layer.
    pub fn replace(&mut self, begin: usize, end: usize, value: T) {
        let begin = begin.min(self.len);
        let end = end.min(self.len);
        if begin >= end {
            return;
        }

        let mut rebuilt = Vec::with_capacity(self.runs.len().saturating_add(2));
        let mut offset = 0usize;
        let mut replacement_emitted = false;

        for run in &self.runs {
            let run_begin = offset;
            let run_end = run_begin + run.length;

            if run_end <= begin || run_begin >= end {
                if !replacement_emitted && run_begin >= end {
                    push_run(&mut rebuilt, value.clone(), end - begin);
                    replacement_emitted = true;
                }
                push_run(&mut rebuilt, run.value.clone(), run.length);
            } else {
                if run_begin < begin {
                    push_run(&mut rebuilt, run.value.clone(), begin - run_begin);
                }

                if !replacement_emitted {
                    push_run(&mut rebuilt, value.clone(), end - begin);
                    replacement_emitted = true;
                }

                if run_end > end {
                    push_run(&mut rebuilt, run.value.clone(), run_end - end);
                }
            }

            offset = run_end;
        }

        if !replacement_emitted {
            push_run(&mut rebuilt, value, end - begin);
        }

        self.runs = rebuilt;
        debug_assert_eq!(
            self.runs.iter().map(|run| run.length).sum::<usize>(),
            self.len
        );
        debug_assert!(self.runs.iter().all(|run| run.length > 0));
        debug_assert!(
            self.runs
                .windows(2)
                .all(|pair| pair[0].value != pair[1].value)
        );
    }

    pub fn fill(&mut self, value: T) {
        if self.len == 0 {
            self.runs.clear();
        } else {
            self.runs = vec![Run {
                value,
                length: self.len,
            }];
        }
    }

    #[must_use]
    pub fn expanded(&self) -> Vec<T> {
        let mut values = Vec::with_capacity(self.len);
        for run in &self.runs {
            values.extend(std::iter::repeat_n(run.value.clone(), run.length));
        }
        values
    }
}

fn push_run<T: Eq>(runs: &mut Vec<Run<T>>, value: T, length: usize) {
    if length == 0 {
        return;
    }

    if let Some(last) = runs.last_mut()
        && last.value == value
    {
        last.length += length;
        return;
    }

    runs.push(Run { value, length });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn construction_is_canonical() {
        let empty = Rle::new(0, 7u8);
        assert!(empty.is_empty());
        assert!(empty.runs().is_empty());

        let values = Rle::new(5, 7u8);
        assert_eq!(values.len(), 5);
        assert_eq!(
            values.runs(),
            &[Run {
                value: 7,
                length: 5
            }]
        );
        assert_eq!(values.expanded(), vec![7, 7, 7, 7, 7]);
    }

    #[test]
    fn at_observes_run_boundaries() {
        let mut values = Rle::new(6, 'a');
        values.replace(2, 4, 'b');

        assert_eq!(values.at(0), Some(&'a'));
        assert_eq!(values.at(1), Some(&'a'));
        assert_eq!(values.at(2), Some(&'b'));
        assert_eq!(values.at(3), Some(&'b'));
        assert_eq!(values.at(4), Some(&'a'));
        assert_eq!(values.at(5), Some(&'a'));
        assert_eq!(values.at(6), None);
    }

    #[test]
    fn replace_splits_and_merges_runs() {
        let mut values = Rle::new(8, 1u8);
        values.replace(2, 6, 2);
        assert_eq!(
            values.runs(),
            &[
                Run {
                    value: 1,
                    length: 2
                },
                Run {
                    value: 2,
                    length: 4
                },
                Run {
                    value: 1,
                    length: 2
                }
            ]
        );

        values.replace(1, 7, 1);
        assert_eq!(
            values.runs(),
            &[Run {
                value: 1,
                length: 8
            }]
        );
        assert_eq!(values.expanded(), vec![1; 8]);
    }

    #[test]
    fn replace_across_multiple_runs_preserves_length() {
        let mut values = Rle::new(10, 0u8);
        values.replace(2, 4, 1);
        values.replace(6, 8, 2);
        values.replace(3, 7, 9);

        assert_eq!(values.len(), 10);
        assert_eq!(values.expanded(), vec![0, 0, 1, 9, 9, 9, 9, 2, 0, 0]);
        assert_eq!(
            values.runs().iter().map(|run| run.length).sum::<usize>(),
            10
        );
    }

    #[test]
    fn replace_clamps_and_empty_ranges_are_noops() {
        let mut values = Rle::new(4, 'x');
        values.replace(4, 100, 'y');
        values.replace(3, 2, 'y');
        assert_eq!(values.expanded(), vec!['x'; 4]);

        values.replace(2, 100, 'y');
        assert_eq!(values.expanded(), vec!['x', 'x', 'y', 'y']);
    }

    #[test]
    fn fill_collapses_existing_runs() {
        let mut values = Rle::new(5, 1u8);
        values.replace(1, 4, 2);
        values.fill(3);

        assert_eq!(
            values.runs(),
            &[Run {
                value: 3,
                length: 5
            }]
        );
        assert_eq!(values.expanded(), vec![3; 5]);
    }
}
