//! Safe parsing of already-tokenized conhost arguments.
//!
//! `CommandLineToArgvW` remains a Windows-owned boundary. Once tokens exist,
//! this module mirrors the deterministic state transitions in
//! `ConsoleArguments::ParseCommandline`.

use crate::console_arguments::join_client_arguments;

const HEADLESS_ARG: &str = "--headless";
const SERVER_HANDLE_ARG: &str = "--server";
const SIGNAL_HANDLE_ARG: &str = "--signal";
const HANDLE_PREFIX: &str = "0x";
const CLIENT_COMMANDLINE_ARG: &str = "--";
const FORCE_V1_ARG: &str = "-ForceV1";
const FORCE_NO_HANDOFF_ARG: &str = "-ForceNoHandoff";
const FILEPATH_LEADER_PREFIX: &str = "\\??\\";
const WIDTH_ARG: &str = "--width";
const HEIGHT_ARG: &str = "--height";
const INHERIT_CURSOR_ARG: &str = "--inheritcursor";
const FEATURE_ARG: &str = "--feature";
const FEATURE_PTY_ARG: &str = "pty";
const COM_SERVER_ARG: &str = "-Embedding";
const TEXT_MEASUREMENT_ARG: &str = "--textMeasurement";
const AMBIGUOUS_IS_WIDE_ARG: &str = "--ambiguousIsWide";

const MODE_AMBIGUOUS_IS_WIDE: u16 = 1 << 0;
const MODE_FORCE_V1: u16 = 1 << 1;
const MODE_FORCE_NO_HANDOFF: u16 = 1 << 2;
const MODE_HEADLESS: u16 = 1 << 3;
const MODE_RUN_AS_COM_SERVER: u16 = 1 << 4;
const MODE_CREATE_SERVER_HANDLE: u16 = 1 << 5;
const MODE_INHERIT_CURSOR: u16 = 1 << 6;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ConsoleArgumentState {
    pub client_commandline: String,
    pub text_measurement: String,
    pub width: i16,
    pub height: i16,
    pub server_handle: Option<u32>,
    pub signal_handle: Option<u32>,
    modes: u16,
}

impl Default for ConsoleArgumentState {
    fn default() -> Self {
        Self {
            client_commandline: String::new(),
            text_measurement: String::new(),
            width: 0,
            height: 0,
            server_handle: None,
            signal_handle: None,
            modes: MODE_CREATE_SERVER_HANDLE,
        }
    }
}

impl ConsoleArgumentState {
    #[must_use]
    pub const fn ambiguous_is_wide(&self) -> bool {
        self.has_mode(MODE_AMBIGUOUS_IS_WIDE)
    }

    #[must_use]
    pub const fn force_v1(&self) -> bool {
        self.has_mode(MODE_FORCE_V1)
    }

    #[must_use]
    pub const fn force_no_handoff(&self) -> bool {
        self.has_mode(MODE_FORCE_NO_HANDOFF)
    }

    #[must_use]
    pub const fn headless(&self) -> bool {
        self.has_mode(MODE_HEADLESS)
    }

    #[must_use]
    pub const fn run_as_com_server(&self) -> bool {
        self.has_mode(MODE_RUN_AS_COM_SERVER)
    }

    #[must_use]
    pub const fn create_server_handle(&self) -> bool {
        self.has_mode(MODE_CREATE_SERVER_HANDLE)
    }

    #[must_use]
    pub const fn inherit_cursor(&self) -> bool {
        self.has_mode(MODE_INHERIT_CURSOR)
    }

    const fn has_mode(&self, mode: u16) -> bool {
        self.modes & mode != 0
    }

    fn set_mode(&mut self, mode: u16) {
        self.modes |= mode;
    }

    fn clear_mode(&mut self, mode: u16) {
        self.modes &= !mode;
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ConsoleArgumentError {
    MissingValue(&'static str),
    InvalidValue(&'static str),
    DuplicateHandle(&'static str),
}

/// Parses conhost arguments after Windows command-line tokenization.
///
/// The input excludes the executable token, matching the C++ `args` vector.
///
/// # Errors
/// Returns an error when a recognized switch is missing its value, contains an
/// invalid value, or attempts to set a server/signal handle more than once.
pub fn parse_console_arguments(
    tokens: &[String],
) -> Result<ConsoleArgumentState, ConsoleArgumentError> {
    let mut state = ConsoleArgumentState::default();
    let mut index = 0usize;

    while index < tokens.len() {
        let arg = tokens[index].as_str();

        if arg.starts_with(HANDLE_PREFIX) || arg == SERVER_HANDLE_ARG {
            let (value, consumed) = if arg == SERVER_HANDLE_ARG {
                (next_value(tokens, index, SERVER_HANDLE_ARG)?, 2)
            } else {
                (arg, 1)
            };
            if state.server_handle.is_some() {
                return Err(ConsoleArgumentError::DuplicateHandle("server"));
            }
            state.server_handle = Some(parse_handle(value)?);
            state.clear_mode(MODE_CREATE_SERVER_HANDLE);
            index += consumed;
        } else if arg == SIGNAL_HANDLE_ARG {
            let value = next_value(tokens, index, SIGNAL_HANDLE_ARG)?;
            if state.signal_handle.is_some() {
                return Err(ConsoleArgumentError::DuplicateHandle("signal"));
            }
            state.signal_handle = Some(parse_handle(value)?);
            index += 2;
        } else if arg == FORCE_V1_ARG {
            state.set_mode(MODE_FORCE_V1);
            index += 1;
        } else if arg == FORCE_NO_HANDOFF_ARG {
            state.set_mode(MODE_FORCE_NO_HANDOFF);
            index += 1;
        } else if arg == COM_SERVER_ARG {
            state.set_mode(MODE_RUN_AS_COM_SERVER);
            index += 1;
        } else if arg.starts_with(FILEPATH_LEADER_PREFIX) {
            index += 1;
        } else if arg == WIDTH_ARG {
            state.width = parse_short(next_value(tokens, index, WIDTH_ARG)?)?;
            index += 2;
        } else if arg == HEIGHT_ARG {
            state.height = parse_short(next_value(tokens, index, HEIGHT_ARG)?)?;
            index += 2;
        } else if arg == FEATURE_ARG {
            let value = next_value(tokens, index, FEATURE_ARG)?;
            if value != FEATURE_PTY_ARG {
                return Err(ConsoleArgumentError::InvalidValue("feature"));
            }
            index += 2;
        } else if arg == HEADLESS_ARG {
            state.set_mode(MODE_HEADLESS);
            index += 1;
        } else if arg == INHERIT_CURSOR_ARG {
            state.set_mode(MODE_INHERIT_CURSOR);
            index += 1;
        } else if arg == TEXT_MEASUREMENT_ARG {
            next_value(tokens, index, TEXT_MEASUREMENT_ARG)?
                .clone_into(&mut state.text_measurement);
            index += 2;
        } else if arg == AMBIGUOUS_IS_WIDE_ARG {
            state.set_mode(MODE_AMBIGUOUS_IS_WIDE);
            index += 1;
        } else if arg == CLIENT_COMMANDLINE_ARG {
            state.client_commandline =
                join_client_arguments(tokens[index + 1..].iter().map(String::as_str));
            break;
        } else {
            state.client_commandline =
                join_client_arguments(tokens[index..].iter().map(String::as_str));
            break;
        }
    }

    Ok(state)
}

fn next_value<'a>(
    tokens: &'a [String],
    index: usize,
    argument: &'static str,
) -> Result<&'a str, ConsoleArgumentError> {
    tokens
        .get(index + 1)
        .map(String::as_str)
        .ok_or(ConsoleArgumentError::MissingValue(argument))
}

fn parse_short(value: &str) -> Result<i16, ConsoleArgumentError> {
    let parsed = value
        .parse::<i32>()
        .map_err(|_| ConsoleArgumentError::InvalidValue("dimension"))?;
    if parsed > i32::from(i16::MAX) {
        return Err(ConsoleArgumentError::InvalidValue("dimension"));
    }

    let wrapped = parsed & 0xffff;
    let unsigned = u16::try_from(wrapped).expect("16-bit mask always fits u16");
    Ok(i16::from_ne_bytes(unsigned.to_ne_bytes()))
}

fn parse_handle(value: &str) -> Result<u32, ConsoleArgumentError> {
    let digits = value
        .strip_prefix(HANDLE_PREFIX)
        .ok_or(ConsoleArgumentError::InvalidValue("handle"))?;

    let mut parsed = 0u32;
    let mut saw_digit = false;
    for digit in digits.chars().take_while(char::is_ascii_hexdigit) {
        saw_digit = true;
        let value = digit.to_digit(16).expect("filtered hexadecimal digit");
        parsed = parsed.saturating_mul(16).saturating_add(value);
    }

    if !saw_digit || parsed == 0 {
        Err(ConsoleArgumentError::InvalidValue("handle"))
    } else {
        Ok(parsed)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tokens(values: &[&str]) -> Vec<String> {
        values.iter().map(|value| (*value).to_owned()).collect()
    }

    #[test]
    fn recognized_switches_populate_state() {
        let parsed = parse_console_arguments(&tokens(&[
            "--server",
            "0x40",
            "--signal",
            "0x50",
            "-ForceV1",
            "-ForceNoHandoff",
            "-Embedding",
            "--width",
            "120",
            "--height",
            "30",
            "--feature",
            "pty",
            "--headless",
            "--inheritcursor",
            "--textMeasurement",
            "graphemes",
            "--ambiguousIsWide",
        ]))
        .expect("valid conhost arguments");

        assert_eq!(parsed.server_handle, Some(0x40));
        assert_eq!(parsed.signal_handle, Some(0x50));
        assert!(!parsed.create_server_handle());
        assert!(parsed.force_v1());
        assert!(parsed.force_no_handoff());
        assert!(parsed.run_as_com_server());
        assert_eq!(parsed.width, 120);
        assert_eq!(parsed.height, 30);
        assert!(parsed.headless());
        assert!(parsed.inherit_cursor());
        assert_eq!(parsed.text_measurement, "graphemes");
        assert!(parsed.ambiguous_is_wide());
    }

    #[test]
    fn legacy_server_handle_is_accepted() {
        let parsed = parse_console_arguments(&tokens(&["0x1234"])).expect("legacy handle");
        assert_eq!(parsed.server_handle, Some(0x1234));
        assert!(!parsed.create_server_handle());
    }

    #[test]
    fn explicit_separator_starts_client_commandline() {
        let parsed =
            parse_console_arguments(&tokens(&["--headless", "--", "cmd.exe", "/c", "echo hi"]))
                .expect("client command line");
        assert!(parsed.headless());
        assert_eq!(parsed.client_commandline, "cmd.exe /c \"echo hi\"");
    }

    #[test]
    fn first_unknown_argument_starts_client_commandline() {
        let parsed = parse_console_arguments(&tokens(&["--headless", "pwsh.exe", "-NoLogo"]))
            .expect("implicit client command line");
        assert!(parsed.headless());
        assert_eq!(parsed.client_commandline, "pwsh.exe -NoLogo");
    }

    #[test]
    fn historical_filepath_leader_is_ignored() {
        let parsed = parse_console_arguments(&tokens(&[
            "\\??\\C:\\Windows\\System32\\conhost.exe",
            "--headless",
        ]))
        .expect("historical path token");
        assert!(parsed.headless());
        assert!(parsed.client_commandline.is_empty());
    }

    #[test]
    fn invalid_feature_and_missing_values_fail() {
        assert_eq!(
            parse_console_arguments(&tokens(&["--feature", "unknown"])),
            Err(ConsoleArgumentError::InvalidValue("feature"))
        );
        assert_eq!(
            parse_console_arguments(&tokens(&["--width"])),
            Err(ConsoleArgumentError::MissingValue("--width"))
        );
    }

    #[test]
    fn handles_require_prefix_nonzero_and_single_assignment() {
        assert_eq!(
            parse_console_arguments(&tokens(&["--server", "40"])),
            Err(ConsoleArgumentError::InvalidValue("handle"))
        );
        assert_eq!(
            parse_console_arguments(&tokens(&["--server", "0x0"])),
            Err(ConsoleArgumentError::InvalidValue("handle"))
        );
        assert_eq!(
            parse_console_arguments(&tokens(&["0x1", "--server", "0x2"])),
            Err(ConsoleArgumentError::DuplicateHandle("server"))
        );
    }

    #[test]
    fn handle_parser_matches_wcstoul_prefix_consumption_and_saturation() {
        assert_eq!(parse_handle("0x4oops"), Ok(0x4));
        assert_eq!(parse_handle("0x1ffffffff"), Ok(u32::MAX));
    }

    #[test]
    fn short_parser_matches_cpp_upper_bound_contract() {
        assert_eq!(parse_short("32767"), Ok(32767));
        assert_eq!(
            parse_short("32768"),
            Err(ConsoleArgumentError::InvalidValue("dimension"))
        );
        assert_eq!(parse_short("-32769"), Ok(32767));
        assert_eq!(
            parse_short("8foo"),
            Err(ConsoleArgumentError::InvalidValue("dimension"))
        );
    }
}
