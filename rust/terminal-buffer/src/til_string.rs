//! Portable owner for the deterministic Microsoft `til::string` contracts.

#[must_use]
pub fn visualize_control_codes(input: &str) -> String {
    input
        .chars()
        .map(|ch| match ch as u32 {
            0x00..=0x1f => char::from_u32(ch as u32 + 0x2400).expect("control picture is valid"),
            0x20 => '\u{2423}',
            0x7f => '\u{2421}',
            _ => ch,
        })
        .collect()
}

#[must_use]
pub fn starts_with(input: &str, prefix: &str) -> bool {
    input.starts_with(prefix)
}

#[must_use]
pub fn ends_with(input: &str, suffix: &str) -> bool {
    input.ends_with(suffix)
}

#[must_use]
pub fn parse_u64(input: &str) -> Option<u64> {
    let bytes = input.as_bytes();
    if bytes.is_empty() {
        return None;
    }

    let mut base = 10u64;
    let mut index = 0usize;
    if bytes.len() >= 2 && bytes[0] == b'0' {
        base = 8;
        index = 1;
        match bytes[1] | 0x20 {
            b'b' => {
                base = 2;
                index = 2;
            }
            b'x' => {
                base = 16;
                index = 2;
            }
            _ => {}
        }
    }

    if index == bytes.len() {
        return None;
    }

    let mut value = 0u64;
    for byte in &bytes[index..] {
        let digit = match byte {
            b'0'..=b'9' => u64::from(byte - b'0'),
            b'A'..=b'Z' => u64::from(byte - b'A') + 10,
            b'a'..=b'z' => u64::from(byte - b'a') + 10,
            _ => return None,
        };
        if digit >= base {
            return None;
        }
        value = value.checked_mul(base)?.checked_add(digit)?;
    }
    Some(value)
}

#[must_use]
pub fn parse_unsigned_u32(input: &str) -> Option<u32> {
    u32::try_from(parse_u64(input)?).ok()
}

#[must_use]
pub fn parse_signed_i32(input: &str) -> Option<i32> {
    let (negative, magnitude_text) = input
        .strip_prefix('-')
        .map_or((false, input), |rest| (true, rest));
    let magnitude = parse_u64(magnitude_text)?;

    if negative {
        let minimum_magnitude = i32::MAX as u64 + 1;
        if magnitude > minimum_magnitude {
            return None;
        }
        if magnitude == minimum_magnitude {
            Some(i32::MIN)
        } else {
            Some(-(magnitude as i32))
        }
    } else {
        i32::try_from(magnitude).ok()
    }
}

#[must_use]
pub fn tolower_ascii(ch: char) -> char {
    if ch.is_ascii_uppercase() {
        char::from_u32(ch as u32 | 0x20).expect("ASCII lowercase is valid")
    } else {
        ch
    }
}

#[must_use]
pub fn toupper_ascii(ch: char) -> char {
    if ch.is_ascii_lowercase() {
        char::from_u32(ch as u32 & !0x20).expect("ASCII uppercase is valid")
    } else {
        ch
    }
}

#[must_use]
pub fn equals_insensitive_ascii(left: &str, right: &str) -> bool {
    left.len() == right.len()
        && left
            .bytes()
            .zip(right.bytes())
            .all(|(left, right)| left.eq_ignore_ascii_case(&right))
}

#[must_use]
pub fn split_preserve_empty(input: &str, delimiter: char) -> Vec<&str> {
    input.split(delimiter).collect()
}

#[must_use]
pub fn clean_filename(input: &str) -> String {
    input
        .chars()
        .filter(|ch| !is_invalid_filename_ascii(*ch))
        .collect()
}

#[must_use]
pub fn clean_path(input: &str) -> String {
    input
        .chars()
        .filter(|ch| !is_invalid_path_ascii(*ch))
        .collect()
}

#[must_use]
pub fn is_legal_path(input: &str) -> bool {
    input.chars().all(|ch| !is_invalid_path_ascii(ch))
}

const fn is_invalid_filename_ascii(ch: char) -> bool {
    matches!(ch, '"' | '*' | '/' | ':' | '<' | '>' | '?' | '\\' | '|')
}

const fn is_invalid_path_ascii(ch: char) -> bool {
    matches!(ch, '"' | '*' | '<' | '>' | '?' | '|')
}

#[must_use]
pub fn iterate_font_families(families: &str) -> Vec<String> {
    let mut result = Vec::new();
    let mut family = String::new();
    let mut escape = false;
    let mut delayed_space = false;
    let mut string_type: Option<char> = None;

    for ch in families.chars() {
        if !escape {
            match ch {
                ' ' if string_type.is_none() => {
                    delayed_space = !family.is_empty();
                    continue;
                }
                '"' | '\'' => {
                    if let Some(quote) = string_type {
                        if quote == ch {
                            string_type = None;
                            continue;
                        }
                    } else {
                        string_type = Some(ch);
                        continue;
                    }
                }
                ',' if string_type.is_none() => {
                    if !family.is_empty() {
                        result.push(std::mem::take(&mut family));
                        delayed_space = false;
                    }
                    continue;
                }
                '\\' => {
                    escape = true;
                    continue;
                }
                _ => {}
            }
        }

        if delayed_space {
            delayed_space = false;
            family.push(' ');
        }
        family.push(ch);
        escape = false;
    }

    if string_type.is_none() && !family.is_empty() {
        result.push(family);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_til_string_visualize_control_codes() {
        assert_eq!(
            visualize_control_codes("\u{1b}[A \u{1b}[B\u{7f}"),
            "\u{241b}[A\u{2423}\u{241b}[B\u{2421}"
        );
    }

    #[test]
    fn microsoft_til_string_starts_with() {
        for (value, prefix, expected) in [
            ("", "", true),
            ("abc", "", true),
            ("abc", "a", true),
            ("abc", "ab", true),
            ("abc", "abc", true),
            ("abc", "abcd", false),
            ("", "abc", false),
            ("a", "abc", false),
            ("ab", "abc", false),
            ("abcd", "abc", true),
        ] {
            assert_eq!(
                starts_with(value, prefix),
                expected,
                "{value:?}, {prefix:?}"
            );
        }
    }

    #[test]
    fn microsoft_til_string_ends_with() {
        for (value, suffix, expected) in [
            ("", "", true),
            ("abc", "", true),
            ("abc", "c", true),
            ("abc", "bc", true),
            ("abc", "abc", true),
            ("abc", "0abc", false),
            ("", "abc", false),
            ("c", "abc", false),
            ("bc", "abc", false),
            ("0abc", "abc", true),
        ] {
            assert_eq!(ends_with(value, suffix), expected, "{value:?}, {suffix:?}");
        }
    }

    #[test]
    fn microsoft_til_string_parse_u64() {
        assert_eq!(parse_u64("0"), Some(0));
        assert_eq!(parse_u64("0123"), Some(0o123));
        assert_eq!(parse_u64("0x123abc"), Some(0x123abc));
        assert_eq!(parse_u64("0X123ABC"), Some(0x123abc));
        assert_eq!(parse_u64("0b101"), Some(5));
        assert_eq!(parse_u64("123abc"), None);
    }

    #[test]
    fn microsoft_til_string_parse_u64_overflow() {
        assert_eq!(parse_u64("18446744073709551614"), Some(u64::MAX - 1));
        assert_eq!(parse_u64("18446744073709551615"), Some(u64::MAX));
        assert_eq!(parse_u64("18446744073709551616"), None);
        assert_eq!(parse_u64("18446744073709551617"), None);
        assert_eq!(parse_u64("88888888888888888888"), None);
    }

    #[test]
    fn microsoft_til_string_parse_unsigned() {
        for invalid in [
            "",
            "0x",
            "Z",
            "0xZ",
            "0Z",
            "123abc",
            "0123abc",
            "0x100000000",
        ] {
            assert_eq!(parse_unsigned_u32(invalid), None, "{invalid:?}");
        }
        assert_eq!(parse_unsigned_u32("0"), Some(0));
        assert_eq!(parse_unsigned_u32("0x0"), Some(0));
        assert_eq!(parse_unsigned_u32("0123"), Some(0o123));
        assert_eq!(parse_unsigned_u32("123"), Some(123));
        assert_eq!(parse_unsigned_u32("0x123"), Some(0x123));
        assert_eq!(parse_unsigned_u32("0xffffffff"), Some(u32::MAX));
        assert_eq!(parse_unsigned_u32("4294967295"), Some(u32::MAX));
    }

    #[test]
    fn microsoft_til_string_parse_signed() {
        for invalid in [
            "",
            "-",
            "--",
            "--0",
            "-0Z",
            "-123abc",
            "-0123abc",
            "0x80000000",
            "-0x80000001",
        ] {
            assert_eq!(parse_signed_i32(invalid), None, "{invalid:?}");
        }
        assert_eq!(parse_signed_i32("0"), Some(0));
        assert_eq!(parse_signed_i32("-0"), Some(0));
        assert_eq!(parse_signed_i32("-0x0"), Some(0));
        assert_eq!(parse_signed_i32("0123"), Some(0o123));
        assert_eq!(parse_signed_i32("-0123"), Some(-0o123));
        assert_eq!(parse_signed_i32("-0x123abc"), Some(-0x123abc));
        assert_eq!(parse_signed_i32("-0x80000000"), Some(i32::MIN));
        assert_eq!(parse_signed_i32("-2147483648"), Some(i32::MIN));
        assert_eq!(parse_signed_i32("0x7fffffff"), Some(i32::MAX));
        assert_eq!(parse_signed_i32("2147483647"), Some(i32::MAX));
    }

    #[test]
    fn microsoft_til_string_tolower_ascii() {
        for code in 0u8..=127 {
            let ch = char::from(code);
            assert_eq!(tolower_ascii(ch), ch.to_ascii_lowercase());
        }
    }

    #[test]
    fn microsoft_til_string_toupper_ascii() {
        for code in 0u8..=127 {
            let ch = char::from(code);
            assert_eq!(toupper_ascii(ch), ch.to_ascii_uppercase());
        }
    }

    #[test]
    fn microsoft_til_string_equals_insensitive_ascii() {
        assert!(equals_insensitive_ascii("", ""));
        assert!(!equals_insensitive_ascii("", "foo"));
        assert!(!equals_insensitive_ascii("foo", "fo"));
        assert!(!equals_insensitive_ascii("fooo", "foo"));
        assert!(equals_insensitive_ascii("cOUnterStriKE", "COuntERStRike"));
    }

    #[test]
    fn microsoft_til_string_split_iterator() {
        assert_eq!(split_preserve_empty("foo", ' '), ["foo"]);
        assert_eq!(split_preserve_empty(" foo", ' '), ["", "foo"]);
        assert_eq!(split_preserve_empty("foo ", ' '), ["foo", ""]);
        assert_eq!(
            split_preserve_empty("foo bar baz", ' '),
            ["foo", "bar", "baz"]
        );
        assert_eq!(
            split_preserve_empty(";;foo;;bar;;", ';'),
            ["", "", "foo", "", "bar", "", ""]
        );
    }

    #[test]
    fn microsoft_til_string_clean_path_and_filename() {
        let input = r#"C:\Users\Geddy\Music\"Analog Man""#;
        assert_eq!(clean_filename(input), "CUsersGeddyMusicAnalog Man");
        assert_eq!(clean_path(input), r"C:\Users\Geddy\Music\Analog Man");
    }

    #[test]
    fn microsoft_til_string_legal_path() {
        assert!(is_legal_path(
            r"C:\Users\Documents and Settings\Users\;\Why not"
        ));
        assert!(!is_legal_path(
            r#"C:\Users\Documents and Settings\"Quote-un-quote users""#
        ));
    }

    #[test]
    fn microsoft_til_string_iterate_font_families() {
        assert_eq!(
            iterate_font_families(r#"  foo  ," b  a  r ",b\"az"#),
            ["foo", " b  a  r ", "b\"az"]
        );
        assert_eq!(iterate_font_families(r#""foo, bar""#), ["foo, bar"]);
        assert_eq!(
            iterate_font_families(r#"'"foo"', "'bar'""#),
            ["\"foo\"", "'bar'"]
        );
        assert_eq!(
            iterate_font_families(r#""\"foo\"", '\'bar\''"#),
            ["\"foo\"", "'bar'"]
        );
        assert_eq!(iterate_font_families(",,,,foo,,,,"), ["foo"]);
    }
}
