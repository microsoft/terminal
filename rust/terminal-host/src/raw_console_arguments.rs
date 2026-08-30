//! Portable reconstruction of the Windows raw-command-line boundary used by conhost.
//!
//! The host receives a raw command line, tokenizes it with Windows quote/backslash
//! rules, discards the executable token, and then applies the deterministic
//! `ConsoleArguments` state machine. Keeping that boundary in safe Rust lets the
//! Microsoft `ConsoleArgumentsTests` raw-command-line contracts run without Win32.

use crate::console_argument_parser::{
    ConsoleArgumentError, ConsoleArgumentState, parse_console_arguments,
};

/// Parses a complete conhost command line, including the executable token.
///
/// # Errors
/// Returns the same deterministic argument errors as `parse_console_arguments`.
pub fn parse_raw_console_arguments(
    command_line: &str,
) -> Result<ConsoleArgumentState, ConsoleArgumentError> {
    let tokens = tokenize_windows_command_line(command_line);
    let arguments = tokens.get(1..).unwrap_or_default();
    parse_console_arguments(arguments)
}

/// Tokenizes the subset of `CommandLineToArgvW` semantics needed by conhost.
///
/// Spaces and tabs delimit outside quotes. Backslashes are literal unless they
/// immediately precede a quote; in that case pairs collapse to backslashes and an
/// odd remainder escapes the quote. Quotes otherwise toggle quoted state.
#[must_use]
pub fn tokenize_windows_command_line(command_line: &str) -> Vec<String> {
    let chars: Vec<char> = command_line.chars().collect();
    let mut tokens = Vec::new();
    let mut index = 0usize;

    while index < chars.len() {
        while index < chars.len() && matches!(chars[index], ' ' | '\t') {
            index += 1;
        }
        if index == chars.len() {
            break;
        }

        let mut token = String::new();
        let mut quoted = false;
        while index < chars.len() {
            if !quoted && matches!(chars[index], ' ' | '\t') {
                break;
            }

            if chars[index] == '\\' {
                let start = index;
                while index < chars.len() && chars[index] == '\\' {
                    index += 1;
                }
                let slashes = index - start;
                if index < chars.len() && chars[index] == '"' {
                    for _ in 0..(slashes / 2) {
                        token.push('\\');
                    }
                    if slashes.is_multiple_of(2) {
                        quoted = !quoted;
                    } else {
                        token.push('"');
                    }
                    index += 1;
                } else {
                    for _ in 0..slashes {
                        token.push('\\');
                    }
                }
                continue;
            }

            if chars[index] == '"' {
                quoted = !quoted;
                index += 1;
                continue;
            }

            token.push(chars[index]);
            index += 1;
        }

        tokens.push(token);
    }

    tokens
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_console_arguments_arg_splitting_contract() {
        let cases = [
            (
                "conhost.exe --headless this is the commandline",
                true,
                None,
                "this is the commandline",
            ),
            (
                "conhost.exe \"this is the commandline\"",
                false,
                None,
                "\"this is the commandline\"",
            ),
            (
                "conhost.exe --headless \"--vtmode bar this is the commandline\"",
                true,
                None,
                "\"--vtmode bar this is the commandline\"",
            ),
            (
                "conhost.exe --headless   --server    0x4       this      is the    commandline",
                true,
                Some(4),
                "this is the commandline",
            ),
            (
                "conhost.exe --headless\tthis\tis\tthe\tcommandline",
                true,
                None,
                "this is the commandline",
            ),
            (
                "conhost.exe this is the commandline",
                false,
                None,
                "this is the commandline",
            ),
        ];

        for (raw, headless, server, client) in cases {
            let parsed = parse_raw_console_arguments(raw).expect("Microsoft vector parses");
            assert_eq!(parsed.headless(), headless, "{raw}");
            assert_eq!(parsed.server_handle, server, "{raw}");
            assert_eq!(parsed.client_commandline, client, "{raw}");
        }
    }

    #[test]
    fn microsoft_console_arguments_client_commandline_contract() {
        let cases = [
            ("conhost.exe -- foo", "foo"),
            ("conhost.exe foo", "foo"),
            ("conhost.exe foo -- bar", "foo -- bar"),
            (
                "conhost.exe console --vtmode foo foo -- bar",
                "console --vtmode foo foo -- bar",
            ),
            (
                "conhost.exe console --vtmode foo --outpipe foo -- bar",
                "console --vtmode foo --outpipe foo -- bar",
            ),
            ("conhost.exe -- --outpipe foo bar", "--outpipe foo bar"),
            ("conhost.exe --", ""),
            ("conhost.exe", ""),
        ];

        for (raw, client) in cases {
            let parsed = parse_raw_console_arguments(raw).expect("Microsoft vector parses");
            assert_eq!(parsed.client_commandline, client, "{raw}");
        }
    }

    #[test]
    fn microsoft_console_arguments_legacy_formats_contract() {
        let parsed = parse_raw_console_arguments("conhost.exe 0x4").expect("legacy handle parses");
        assert_eq!(parsed.server_handle, Some(4));
        assert!(!parsed.create_server_handle());

        let parsed = parse_raw_console_arguments(
            "conhost.exe \\??\\C:\\Windows\\System32\\conhost.exe --headless",
        )
        .expect("legacy filepath leader parses");
        assert!(parsed.headless());
        assert!(parsed.client_commandline.is_empty());
    }

    #[test]
    fn tokenizer_preserves_windows_quote_and_backslash_boundary() {
        assert_eq!(
            tokenize_windows_command_line("conhost.exe \"a b\" c"),
            ["conhost.exe", "a b", "c"]
        );
        assert_eq!(
            tokenize_windows_command_line("conhost.exe a\\\"b"),
            ["conhost.exe", "a\"b"]
        );
    }
}
