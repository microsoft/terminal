//! Deterministic command-line helpers from host `ConsoleArguments`.
//!
//! Windows Terminal rebuilds the client command line after consuming host-only
//! switches. This module preserves the escaping contract independently of
//! `CommandLineToArgvW`, handles, and other Win32-owned parsing concerns.

/// Escapes one client argument using the same rules as host `EscapeArgument`.
///
/// Empty arguments become `""`. Arguments containing spaces or tabs are quoted.
/// Backslashes preceding a quote, and trailing backslashes inside a quoted
/// argument, are doubled so Windows command-line tokenization reconstructs the
/// original value.
#[must_use]
pub fn escape_argument(argument: &str) -> String {
    if argument.is_empty() {
        return "\"\"".to_owned();
    }

    let has_whitespace = argument.chars().any(|c| matches!(c, ' ' | '\t'));
    let needs_escaping = has_whitespace || argument.chars().any(|c| matches!(c, '"' | '\\'));
    if !needs_escaping {
        return argument.to_owned();
    }

    let mut escaped = String::with_capacity(argument.len() + 2);
    if has_whitespace {
        escaped.push('"');
    }

    let mut slashes = 0usize;
    for c in argument.chars() {
        match c {
            '\\' => {
                slashes += 1;
                escaped.push('\\');
            }
            '"' => {
                for _ in 0..slashes {
                    escaped.push('\\');
                }
                escaped.push('\\');
                escaped.push('"');
                slashes = 0;
            }
            _ => {
                slashes = 0;
                escaped.push(c);
            }
        }
    }

    if has_whitespace {
        for _ in 0..slashes {
            escaped.push('\\');
        }
        escaped.push('"');
    }

    escaped
}

/// Rebuilds the client command line from already-tokenized arguments.
#[must_use]
pub fn join_client_arguments<'a>(arguments: impl IntoIterator<Item = &'a str>) -> String {
    arguments
        .into_iter()
        .map(escape_argument)
        .collect::<Vec<_>>()
        .join(" ")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_argument_is_explicitly_quoted() {
        assert_eq!(escape_argument(""), "\"\"");
    }

    #[test]
    fn simple_argument_is_unchanged() {
        assert_eq!(escape_argument("cmd.exe"), "cmd.exe");
        assert_eq!(escape_argument("日本語"), "日本語");
    }

    #[test]
    fn whitespace_requires_quotes() {
        assert_eq!(escape_argument("hello world"), "\"hello world\"");
        assert_eq!(escape_argument("hello\tworld"), "\"hello\tworld\"");
    }

    #[test]
    fn quote_is_backslash_escaped() {
        assert_eq!(escape_argument("a\"b"), "a\\\"b");
    }

    #[test]
    fn backslashes_before_quote_are_doubled() {
        assert_eq!(escape_argument("a\\\"b"), "a\\\\\\\"b");
        assert_eq!(escape_argument("a\\\\\"b"), "a\\\\\\\\\\\"b");
    }

    #[test]
    fn trailing_backslashes_are_doubled_inside_quotes() {
        assert_eq!(
            escape_argument("C:\\Program Files\\"),
            "\"C:\\Program Files\\\\\""
        );
    }

    #[test]
    fn unquoted_backslash_does_not_change_value() {
        assert_eq!(escape_argument("C:\\Windows"), "C:\\Windows");
    }

    #[test]
    fn client_command_line_escapes_each_argument_independently() {
        assert_eq!(
            join_client_arguments(["cmd.exe", "/c", "echo hello", ""]),
            "cmd.exe /c \"echo hello\" \"\""
        );
    }
}
