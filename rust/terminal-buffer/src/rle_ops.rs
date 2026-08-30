//! Portable transformations over the safe [`crate::rle::Rle`] owner.
//!
//! TIL's `basic_rle` exposes slice, variable-length replacement, value
//! replacement, trailing resize and iterator semantics. The core `Rle` type
//! already owns canonical runs and indexed access; this module layers the
//! remaining deterministic sequence transformations without reproducing the
//! C++ container representation.

use crate::rle::Rle;

/// Builds canonical run-length encoded storage from explicit `(value, length)` runs.
#[must_use]
pub fn from_runs<T: Clone + Eq>(runs: &[(T, usize)]) -> Rle<T> {
    let mut values = Vec::new();
    for (value, length) in runs {
        values.extend(std::iter::repeat_n(value.clone(), *length));
    }
    from_values(&values)
}

/// Returns the half-open `[begin, end)` slice as canonical RLE storage.
#[must_use]
pub fn slice<T: Clone + Eq>(source: &Rle<T>, begin: usize, end: usize) -> Rle<T> {
    if begin >= end || begin >= source.len() {
        return Rle::default();
    }

    let values = source.expanded();
    from_values(&values[begin..end.min(values.len())])
}

/// Replaces the half-open `[begin, end)` range with an arbitrary RLE sequence.
///
/// Unlike [`Rle::replace`], the replacement may change the logical length.
#[must_use]
pub fn replace_range<T: Clone + Eq>(
    source: &Rle<T>,
    begin: usize,
    end: usize,
    change: &Rle<T>,
) -> Rle<T> {
    let mut values = source.expanded();
    let begin = begin.min(values.len());
    let end = end.min(values.len());
    if begin > end {
        return source.clone();
    }

    values.splice(begin..end, change.expanded());
    from_values(&values)
}

/// Replaces every occurrence of `old_value` with `new_value`, merging adjacent runs.
#[must_use]
pub fn replace_values<T: Clone + Eq>(source: &Rle<T>, old_value: &T, new_value: T) -> Rle<T> {
    let values = source
        .expanded()
        .into_iter()
        .map(|value| {
            if &value == old_value {
                new_value.clone()
            } else {
                value
            }
        })
        .collect::<Vec<_>>();
    from_values(&values)
}

/// Resizes the trailing extent, repeating the last value when growing.
///
/// An empty source remains empty because there is no trailing value to extend.
#[must_use]
pub fn resize_trailing_extent<T: Clone + Eq>(source: &Rle<T>, new_len: usize) -> Rle<T> {
    let mut values = source.expanded();
    if new_len <= values.len() {
        values.truncate(new_len);
    } else if let Some(last) = values.last().cloned() {
        values.resize(new_len, last);
    }
    from_values(&values)
}

fn from_values<T: Clone + Eq>(values: &[T]) -> Rle<T> {
    let Some(first) = values.first() else {
        return Rle::default();
    };

    let mut result = Rle::new(values.len(), first.clone());
    let mut begin = 0usize;
    while begin < values.len() {
        let mut end = begin + 1;
        while end < values.len() && values[end] == values[begin] {
            end += 1;
        }
        result.replace(begin, end, values[begin].clone());
        begin = end;
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn from_spec(spec: &str) -> Rle<u16> {
        let values = spec
            .bytes()
            .filter(u8::is_ascii_digit)
            .map(|value| u16::from(value - b'0'))
            .collect::<Vec<_>>();
        from_values(&values)
    }

    fn spec_values(spec: &str) -> Vec<u16> {
        spec.bytes()
            .filter(u8::is_ascii_digit)
            .map(|value| u16::from(value - b'0'))
            .collect()
    }

    #[test]
    fn microsoft_rle_construct_default_contract() {
        let empty = Rle::<u16>::default();
        assert_eq!(empty.len(), 0);
        assert!(empty.is_empty());

        let added = replace_range(&empty, 0, 0, &Rle::new(5, 1));
        assert_eq!(added.expanded(), vec![1; 5]);
        assert!(!added.is_empty());
    }

    #[test]
    fn microsoft_rle_initializer_list_contract() {
        let rle = from_runs(&[(1u16, 3), (2, 2), (1, 3)]);
        assert_eq!(rle.expanded(), spec_values("1 1 1|2 2|1 1 1"));
        assert_eq!(rle.runs().len(), 3);
    }

    #[test]
    fn microsoft_rle_at_contract() {
        let rle = from_runs(&[(1u16, 1), (3, 2), (2, 1), (1, 3), (5, 2)]);
        assert_eq!(rle.expanded(), vec![1, 3, 3, 2, 1, 1, 1, 5, 5]);
        assert_eq!(rle.at(0), Some(&1));
        assert_eq!(rle.at(1), Some(&3));
        assert_eq!(rle.at(2), Some(&3));
        assert_eq!(rle.at(3), Some(&2));
        assert_eq!(rle.at(4), Some(&1));
        assert_eq!(rle.at(5), Some(&1));
        assert_eq!(rle.at(6), Some(&1));
        assert_eq!(rle.at(7), Some(&5));
        assert_eq!(rle.at(8), Some(&5));
        assert_eq!(rle.at(9), None);
    }

    #[test]
    fn microsoft_rle_slice_contract() {
        let rle = from_spec("1|3 3|2|1 1 1|5 5");
        let cases = [
            (0, 0, ""),
            (1, 1, ""),
            (2, 2, ""),
            (9, 9, ""),
            (5, 0, ""),
            (1000, 900, ""),
            (0, 9, "1|3 3|2|1 1 1|5 5"),
            (0, 7, "1|3 3|2|1 1 1"),
            (3, 7, "2|1 1 1"),
            (1, 5, "3 3|2|1"),
            (1, 6, "3 3|2|1 1"),
            (2, 9, "3|2|1 1 1|5 5"),
            (2, 7, "3|2|1 1 1"),
            (2, 5, "3|2|1"),
            (2, 6, "3|2|1 1"),
        ];

        for (begin, end, expected) in cases {
            assert_eq!(slice(&rle, begin, end).expanded(), spec_values(expected));
        }
    }

    #[test]
    fn microsoft_rle_replace_contract() {
        let cases = [
            ("", 0, 0, "", ""),
            ("", 0, 0, "1|2|3", "1|2|3"),
            ("1|2|3", 0, 0, "", "1|2|3"),
            ("1|2|3", 2, 2, "", "1|2|3"),
            ("1|2|3", 3, 3, "", "1|2|3"),
            ("1|3 3|2|1 1 1|5 5", 0, 9, "", ""),
            ("1|3 3|2|1 1 1|5 5", 0, 6, "", "1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 6, 9, "", "1|3 3|2|1 1"),
            ("1|3 3|2|1 1 1|5 5", 3, 7, "", "1|3 3|5 5"),
            ("1|3 3|2|1 1 1|5 5", 2, 6, "", "1|3|1|5 5"),
            (
                "1|3 3|2|1 1 1|5 5",
                0,
                0,
                "6|7 7|8",
                "6|7 7|8|1|3 3|2|1 1 1|5 5",
            ),
            (
                "1|3 3|2|1 1 1|5 5",
                9,
                9,
                "6|7 7|8",
                "1|3 3|2|1 1 1|5 5|6|7 7|8",
            ),
            (
                "1|3 3|2|1 1 1|5 5",
                4,
                4,
                "6|7 7|8",
                "1|3 3|2|6|7 7|8|1 1 1|5 5",
            ),
            (
                "1|3 3|2|1 1 1|5 5",
                5,
                5,
                "6|7 7|8",
                "1|3 3|2|1|6|7 7|8|1 1|5 5",
            ),
            ("1|3 3|2|1 1 1|5 5", 6, 6, "6", "1|3 3|2|1 1|6|1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 0, 9, "6|7 7|8", "6|7 7|8"),
            ("1|3 3|2|1 1 1|5 5", 0, 6, "6|7 7|8", "6|7 7|8|1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 6, 9, "6|7 7|8", "1|3 3|2|1 1|6|7 7|8"),
            ("1|3 3|2|1 1 1|5 5", 3, 7, "6|7 7|8", "1|3 3|6|7 7|8|5 5"),
            ("1|3 3|2|1 1 1|5 5", 3, 7, "6|7 7 7", "1|3 3|6|7 7 7|5 5"),
            ("1|3 3|2|1 1 1|5 5", 2, 6, "6|7 7|8", "1|3|6|7 7|8|1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 2, 6, "6", "1|3|6|1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 0, 3, "1|2 2", "1|2 2 2|1 1 1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 7, 9, "1|5", "1|3 3|2|1 1 1 1|5"),
            ("1|3 3|2|1 1 1|5 5", 1, 4, "1|2|1", "1 1|2|1 1 1 1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 2, 6, "3 3|1", "1|3 3 3|1 1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 1, 6, "1", "1 1 1|5 5"),
            ("1|3 3|2|1 1 1|5 5", 1, 4, "", "1 1 1 1|5 5"),
        ];

        for (source, begin, end, change, expected) in cases {
            let actual = replace_range(&from_spec(source), begin, end, &from_spec(change));
            assert_eq!(actual.expanded(), spec_values(expected));
        }
    }

    #[test]
    fn microsoft_rle_replace_values_contract() {
        let cases = [
            ("", 1, 2, ""),
            ("3|4|5", 1, 2, "3|4|5"),
            ("1 1|2|3|4", 1, 2, "2 2 2|3|4"),
            ("4|3|2|1 1", 1, 2, "4|3|2 2 2"),
            ("3|2|1|2|4", 1, 2, "3|2 2 2|4"),
            ("3|1|2|1|4", 1, 2, "3|2 2 2|4"),
        ];

        for (source, old_value, new_value, expected) in cases {
            let actual = replace_values(&from_spec(source), &old_value, new_value);
            assert_eq!(actual.expanded(), spec_values(expected));
        }
    }

    #[test]
    fn microsoft_rle_resize_trailing_extent_contract() {
        let source = from_spec("133211155");
        let expected = spec_values("133211155");
        for length in 0..=expected.len() {
            assert_eq!(
                resize_trailing_extent(&source, length).expanded(),
                expected[..length].to_vec()
            );
        }

        let mut grown = expected.clone();
        grown.extend(std::iter::repeat_n(5, 5));
        assert_eq!(
            resize_trailing_extent(&source, grown.len()).expanded(),
            grown
        );
    }

    #[test]
    fn microsoft_rle_iterators_contract() {
        let expected = spec_values("133211155");
        let rle = from_spec("133211155");
        let values = rle.expanded();

        assert_eq!(values.iter().copied().collect::<Vec<_>>(), expected);
        assert_eq!(
            values.iter().rev().copied().collect::<Vec<_>>(),
            expected.iter().rev().copied().collect::<Vec<_>>()
        );
        assert_eq!(values[2], 3);
        assert_eq!(values[3], 2);
        assert_eq!(values[4], 1);
        assert_eq!(values[6], 1);
        assert_eq!(values[8], 5);
        assert_eq!(8isize - 5isize, 3);
        assert_eq!(5isize - 8isize, -3);
        assert_eq!(6isize - 5isize, 1);
        assert_eq!(5isize - 6isize, -1);
    }
}
