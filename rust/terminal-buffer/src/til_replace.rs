//! Portable TIL replacement helpers.
//!
//! The C++ `til::replace_needle_in_haystack` family is deterministic and does
//! not depend on Win32. This module owns the equivalent narrow-string and
//! UTF-16 replacement behavior in safe Rust.

/// Returns a new UTF-8 string with all non-overlapping occurrences replaced.
#[must_use]
pub fn replace_all_str(haystack: &str, needle: &str, replacement: &str) -> String {
    haystack.replace(needle, replacement)
}

/// Replaces all non-overlapping occurrences in a UTF-8 string in place.
pub fn replace_all_str_in_place(haystack: &mut String, needle: &str, replacement: &str) {
    *haystack = replace_all_str(haystack, needle, replacement);
}

/// Returns a new UTF-16 code-unit sequence with all non-overlapping occurrences replaced.
#[must_use]
pub fn replace_all_utf16(haystack: &[u16], needle: &[u16], replacement: &[u16]) -> Vec<u16> {
    if needle.is_empty() {
        return haystack.to_vec();
    }

    let mut result = Vec::with_capacity(haystack.len());
    let mut index = 0usize;
    while index < haystack.len() {
        if haystack[index..].starts_with(needle) {
            result.extend_from_slice(replacement);
            index += needle.len();
        } else {
            result.push(haystack[index]);
            index += 1;
        }
    }
    result
}

/// Replaces all non-overlapping occurrences in a UTF-16 code-unit sequence in place.
pub fn replace_all_utf16_in_place(haystack: &mut Vec<u16>, needle: &[u16], replacement: &[u16]) {
    *haystack = replace_all_utf16(haystack, needle, replacement);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn wide(value: &str) -> Vec<u16> {
        value.encode_utf16().collect()
    }

    #[test]
    fn microsoft_replace_strings_contract() {
        let foo = String::from("foo");
        let temp1 = replace_all_str(&foo, "f", "b");
        assert_eq!(temp1, "boo");
        let temp2 = replace_all_str(&temp1, "o", "00");
        assert_eq!(temp2, "b0000");
    }

    #[test]
    fn microsoft_replace_string_and_views_contract() {
        let foo = String::from("foo");
        let f = "f";
        let b = "b";
        let o = "o";
        let zero_zero = "00";
        let temp1 = replace_all_str(&foo, f, b);
        assert_eq!(temp1, "boo");
        let temp2 = replace_all_str(&temp1, o, zero_zero);
        assert_eq!(temp2, "b0000");
    }

    #[test]
    fn microsoft_replace_strings_inplace_contract() {
        let mut foo = String::from("foo");
        replace_all_str_in_place(&mut foo, "f", "b");
        assert_eq!(foo, "boo");
        replace_all_str_in_place(&mut foo, "o", "00");
        assert_eq!(foo, "b0000");
    }

    #[test]
    fn microsoft_replace_string_and_views_inplace_contract() {
        let mut foo = String::from("foo");
        let f = "f";
        let b = "b";
        let o = "o";
        let zero_zero = "00";
        replace_all_str_in_place(&mut foo, f, b);
        assert_eq!(foo, "boo");
        replace_all_str_in_place(&mut foo, o, zero_zero);
        assert_eq!(foo, "b0000");
    }

    #[test]
    fn microsoft_replace_wstrings_contract() {
        let foo = wide("foo");
        let temp1 = replace_all_utf16(&foo, &wide("f"), &wide("b"));
        assert_eq!(temp1, wide("boo"));
        let temp2 = replace_all_utf16(&temp1, &wide("o"), &wide("00"));
        assert_eq!(temp2, wide("b0000"));
    }

    #[test]
    fn microsoft_replace_wstring_and_views_contract() {
        let foo = wide("foo");
        let f = wide("f");
        let b = wide("b");
        let o = wide("o");
        let zero_zero = wide("00");
        let temp1 = replace_all_utf16(&foo, &f, &b);
        assert_eq!(temp1, wide("boo"));
        let temp2 = replace_all_utf16(&temp1, &o, &zero_zero);
        assert_eq!(temp2, wide("b0000"));
    }

    #[test]
    fn microsoft_replace_wstrings_inplace_contract() {
        let mut foo = wide("foo");
        replace_all_utf16_in_place(&mut foo, &wide("f"), &wide("b"));
        assert_eq!(foo, wide("boo"));
        replace_all_utf16_in_place(&mut foo, &wide("o"), &wide("00"));
        assert_eq!(foo, wide("b0000"));
    }

    #[test]
    fn microsoft_replace_wstring_and_views_inplace_contract() {
        let mut foo = wide("foo");
        let f = wide("f");
        let b = wide("b");
        let o = wide("o");
        let zero_zero = wide("00");
        replace_all_utf16_in_place(&mut foo, &f, &b);
        assert_eq!(foo, wide("boo"));
        replace_all_utf16_in_place(&mut foo, &o, &zero_zero);
        assert_eq!(foo, wide("b0000"));
    }
}
