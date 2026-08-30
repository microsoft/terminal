//! Portable deterministic helpers owned by the Windows Terminal `types` layer.

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct XTermColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl XTermColor {
    #[must_use]
    pub const fn new(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct WslStartingDirectoryResult {
    pub command_line: String,
    pub starting_directory: String,
}

#[must_use]
pub fn clamp_to_short_max(value: i32, minimum: i16) -> i16 {
    i16::try_from(value.clamp(i32::from(minimum), i32::from(i16::MAX))).unwrap_or(i16::MAX)
}

#[must_use]
pub fn split_string(input: &str, delimiter: char) -> Vec<&str> {
    if input.is_empty() {
        Vec::new()
    } else {
        input.split(delimiter).collect()
    }
}

#[must_use]
pub fn string_to_uint(input: &str) -> Option<u32> {
    if input.is_empty() {
        return None;
    }

    let mut value = 0u32;
    for byte in input.bytes() {
        if !byte.is_ascii_digit() {
            return None;
        }
        value = value.wrapping_mul(10).wrapping_add(u32::from(byte - b'0'));
    }
    Some(value)
}

#[must_use]
pub fn filter_string_for_paste(
    input: &str,
    carriage_return_newline: bool,
    control_codes: bool,
) -> String {
    let mut filtered = String::with_capacity(input.len());

    for ch in input.chars() {
        if carriage_return_newline && ch == '\n' {
            if !filtered.ends_with('\r') {
                filtered.push('\r');
            }
            continue;
        }

        let code = u32::from(ch);
        let removable_control = control_codes
            && ((code < 0x20 || (0x7f..=0x9f).contains(&code))
                && !matches!(ch, '\t' | '\n' | '\r'));
        if !removable_control {
            filtered.push(ch);
        }
    }

    filtered
}

#[must_use]
pub fn trim_paste(input: &str) -> &str {
    let is_trim_whitespace =
        |ch: char| matches!(ch, '\t' | '\n' | '\u{000b}' | '\u{000c}' | '\r' | ' ');
    let is_newline = |ch: char| matches!(ch, '\n' | '\u{000b}' | '\u{000c}' | '\r');

    let Some((last_non_space, last_char)) = input
        .char_indices()
        .rev()
        .find(|(_, ch)| !is_trim_whitespace(*ch))
    else {
        return "";
    };
    let end = last_non_space + last_char.len_utf8();

    if input[..end].find(is_newline).is_some() {
        input
    } else {
        &input[..end]
    }
}

#[must_use]
pub fn evaluate_starting_directory(current_directory: &str, starting_directory: &str) -> String {
    let bytes = starting_directory.as_bytes();
    let absolute_windows = bytes.len() >= 3 && bytes[1] == b':' && matches!(bytes[2], b'\\' | b'/');
    if absolute_windows
        || starting_directory.starts_with('~')
        || starting_directory.starts_with('/')
    {
        starting_directory.to_owned()
    } else {
        format!("{current_directory}\\{starting_directory}")
    }
}

/// Parses the XTerm/XParseColor formats and X11 application color names used by
/// Terminal settings.
#[must_use]
pub fn color_from_xterm_color(input: &str) -> Option<XTermColor> {
    parse_x_color_spec(input).or_else(|| parse_x11_app_color_name(input))
}

fn parse_x_color_spec(input: &str) -> Option<XTermColor> {
    if !input.is_ascii() {
        return None;
    }

    if input.len() > 4 && input[..4].eq_ignore_ascii_case("rgb:") {
        if !(9..=18).contains(&input.len()) {
            return None;
        }
        let mut components = input[4..].split('/');
        let red = scale_x_component(components.next()?, false)?;
        let green = scale_x_component(components.next()?, false)?;
        let blue = scale_x_component(components.next()?, false)?;
        if components.next().is_some() {
            return None;
        }
        return Some(XTermColor::new(red, green, blue));
    }

    if let Some(body) = input.strip_prefix('#') {
        if !matches!(input.len(), 4 | 7 | 10 | 13) {
            return None;
        }
        let digits = body.len() / 3;
        let red = scale_x_component(&body[..digits], true)?;
        let green = scale_x_component(&body[digits..digits * 2], true)?;
        let blue = scale_x_component(&body[digits * 2..], true)?;
        return Some(XTermColor::new(red, green, blue));
    }

    None
}

fn scale_x_component(component: &str, sharp: bool) -> Option<u8> {
    if component.is_empty()
        || component.len() > 4
        || !component.bytes().all(|byte| byte.is_ascii_hexdigit())
    {
        return None;
    }
    let value = u32::from_str_radix(component, 16).ok()?;
    let digits = u32::try_from(component.len()).ok()?;
    let multiplier = if sharp { 0x10_u32 } else { 0x11_u32 };
    let divisor = (multiplier << 8) >> (4 * (4 - digits));
    u8::try_from(value * multiplier / divisor).ok()
}

fn parse_x11_app_color_name(input: &str) -> Option<XTermColor> {
    if !input.is_ascii() {
        return None;
    }

    let mut stem = String::new();
    let mut variant = 0usize;
    let mut found_variant = false;
    for byte in input.bytes() {
        if byte.is_ascii_digit() {
            found_variant = true;
            variant = variant
                .checked_mul(10)?
                .checked_add(usize::from(byte - b'0'))?;
            continue;
        }
        if byte.is_ascii_whitespace() {
            continue;
        }
        if found_variant {
            return None;
        }
        stem.push(char::from(byte.to_ascii_lowercase()));
    }

    if (stem == "gray" || stem == "grey") && found_variant {
        if variant > 100 {
            return None;
        }
        let component = u8::try_from((variant * 255 + 50) / 100).ok()?;
        return Some(XTermColor::new(component, component, component));
    }

    let variants = match stem.as_str() {
        "yellow" => Some([
            XTermColor::new(255, 255, 0),
            XTermColor::new(255, 255, 0),
            XTermColor::new(238, 238, 0),
            XTermColor::new(205, 205, 0),
            XTermColor::new(139, 139, 0),
        ]),
        "wheat" => Some([
            XTermColor::new(245, 222, 179),
            XTermColor::new(255, 231, 186),
            XTermColor::new(238, 216, 174),
            XTermColor::new(205, 186, 150),
            XTermColor::new(139, 126, 102),
        ]),
        "royalblue" => Some([
            XTermColor::new(65, 105, 225),
            XTermColor::new(72, 118, 255),
            XTermColor::new(67, 110, 238),
            XTermColor::new(58, 95, 205),
            XTermColor::new(39, 64, 139),
        ]),
        _ => None,
    };
    if let Some(colors) = variants {
        return colors.get(variant).copied();
    }

    if found_variant {
        return None;
    }

    match stem.as_str() {
        "orange" => Some(XTermColor::new(255, 165, 0)),
        "darkgreen" => Some(XTermColor::new(0, 100, 0)),
        "mediumseagreen" => Some(XTermColor::new(60, 179, 113)),
        "lightyellow" => Some(XTermColor::new(255, 255, 224)),
        "gray" | "grey" => Some(XTermColor::new(190, 190, 190)),
        _ => None,
    }
}

/// Promotes a Windows Terminal starting directory to WSL's `--cd` argument.
///
/// Platform lookups are injected so the deterministic decision logic remains
/// portable: `system_directory` corresponds to `GetSystemDirectoryW`, and
/// `user_profile` to `%USERPROFILE%` expansion.
#[must_use]
pub fn mangle_starting_directory_for_wsl(
    command_line: &str,
    starting_directory: &str,
    system_directory: &str,
    user_profile: &str,
) -> WslStartingDirectoryResult {
    let fallback = || WslStartingDirectoryResult {
        command_line: command_line.to_owned(),
        starting_directory: if starting_directory == "~" {
            user_profile.to_owned()
        } else {
            starting_directory.to_owned()
        },
    };

    if starting_directory.is_empty() || command_line.len() < 3 {
        return fallback();
    }

    let bytes = command_line.as_bytes();
    let terminator = bytes
        .iter()
        .enumerate()
        .skip(1)
        .find_map(|(index, byte)| matches!(*byte, b'"' | b' ').then_some(index));
    let start = usize::from(command_line.starts_with('"'));
    let end = terminator.unwrap_or(command_line.len());
    if start >= end || !command_line.is_char_boundary(start) || !command_line.is_char_boundary(end)
    {
        return fallback();
    }

    let executable = &command_line[start..end];
    let filename = executable.rsplit(['\\', '/']).next().unwrap_or(executable);
    if !matches!(filename, "wsl" | "wsl.exe") {
        return fallback();
    }

    if let Some(parent_end) = executable.rfind(['\\', '/']) {
        let parent = executable[..parent_end].trim_end_matches(['\\', '/']);
        if !parent.eq_ignore_ascii_case(system_directory.trim_end_matches(['\\', '/'])) {
            return fallback();
        }
    }

    let arguments = terminator.map_or("", |index| &command_line[index.saturating_add(1)..]);
    if arguments.contains("--cd") {
        return fallback();
    }
    if let Some(tilde) = arguments.find('~')
        && (tilde + 1 == arguments.len() || arguments.as_bytes().get(tilde + 1) == Some(&b' '))
    {
        return fallback();
    }

    let mangled_directory = if starting_directory.starts_with("//wsl$")
        || starting_directory.starts_with("//wsl.localhost")
    {
        starting_directory.replace('/', "\\")
    } else {
        starting_directory.to_owned()
    };

    WslStartingDirectoryResult {
        command_line: format!("\"{executable}\" --cd \"{mangled_directory}\" {arguments}"),
        starting_directory: String::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_types_clamp_to_short_max_contract() {
        assert_eq!(clamp_to_short_max(0, 1), 1);
        assert_eq!(clamp_to_short_max(-1, 1), 1);
        assert_eq!(clamp_to_short_max(50_000, 1), i16::MAX);
        assert_eq!(clamp_to_short_max(100, 1), 100);
    }

    #[test]
    fn microsoft_types_split_string_contract() {
        assert!(split_string("", ';').is_empty());
        assert_eq!(split_string("1", ';'), ["1"]);
        assert_eq!(split_string(";", ';'), ["", ""]);
        assert_eq!(split_string("123", ';'), ["123"]);
        assert_eq!(split_string(";123", ';'), ["", "123"]);
        assert_eq!(split_string("123;", ';'), ["123", ""]);
        assert_eq!(split_string("123;456", ';'), ["123", "456"]);
        assert_eq!(split_string("123;456;789", ';'), ["123", "456", "789"]);
    }

    #[test]
    fn microsoft_types_filter_string_for_paste_contract() {
        for (input, expected) in [
            ("Hello World", "Hello World"),
            ("Hello World\r", "Hello World\r"),
            ("Hello World\n", "Hello World\r"),
            ("Hello World\r\n", "Hello World\r"),
            ("Hello\rWorld\r", "Hello\rWorld\r"),
            ("Hello\nWorld\n", "Hello\rWorld\r"),
            ("Hello\r\nWorld\r\n", "Hello\rWorld\r"),
            ("Hello\nWorld\n123", "Hello\rWorld\r123"),
        ] {
            assert_eq!(filter_string_for_paste(input, true, false), expected);
        }

        let c0 = format!(
            "Hello{}{}{} 123",
            char::from(1),
            char::from(2),
            char::from(3)
        );
        assert_eq!(filter_string_for_paste(&c0, false, true), "Hello 123");
        let c1 = format!("echo{}", char::from_u32(0x9c).expect("valid C1 scalar"));
        assert_eq!(filter_string_for_paste(&c1, true, true), "echo");
        let unicode = format!("你好\r\n{}世界{}\r\n123", char::from(1), char::from(2));
        assert_eq!(
            filter_string_for_paste(&unicode, true, true),
            "你好\r世界\r123"
        );
    }

    #[test]
    fn microsoft_types_string_to_uint_contract() {
        assert_eq!(string_to_uint(""), None);
        assert_eq!(string_to_uint("xyz"), None);
        assert_eq!(string_to_uint(";"), None);
        assert_eq!(string_to_uint("1"), Some(1));
        assert_eq!(string_to_uint("123"), Some(123));
        assert_eq!(string_to_uint("123456789"), Some(123_456_789));
    }

    #[test]
    fn microsoft_types_color_from_xterm_color_contract() {
        let rgb = |r, g, b| Some(XTermColor::new(r, g, b));
        for (input, expected) in [
            ("rgb:1/1/1", rgb(0x11, 0x11, 0x11)),
            ("rGb:1/1/1", rgb(0x11, 0x11, 0x11)),
            ("RGB:1/1/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:111/1/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:1111/1/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/11/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/111/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/1111/1", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/1/11", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/1/111", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/1/1111", rgb(0x11, 0x11, 0x11)),
            ("rgb:1/23/4", rgb(0x11, 0x23, 0x44)),
            ("rgb:1/23/45", rgb(0x11, 0x23, 0x45)),
            ("rgb:1/23/456", rgb(0x11, 0x23, 0x45)),
            ("rgb:12/34/5", rgb(0x12, 0x34, 0x55)),
            ("rgb:12/34/56", rgb(0x12, 0x34, 0x56)),
            ("rgb:12/345/67", rgb(0x12, 0x34, 0x67)),
            ("rgb:12/345/678", rgb(0x12, 0x34, 0x67)),
            ("rgb:123/456/789", rgb(0x12, 0x45, 0x78)),
            ("rgb:123/4564/789", rgb(0x12, 0x45, 0x78)),
            ("rgb:123/4564/7897", rgb(0x12, 0x45, 0x78)),
            ("rgb:1231/4564/7897", rgb(0x12, 0x45, 0x78)),
            ("#111", rgb(0x10, 0x10, 0x10)),
            ("#123456", rgb(0x12, 0x34, 0x56)),
            ("#123456789", rgb(0x12, 0x45, 0x78)),
            ("#123145647897", rgb(0x12, 0x45, 0x78)),
            ("orange", rgb(255, 165, 0)),
            ("dark green", rgb(0, 100, 0)),
            ("medium sea green", rgb(60, 179, 113)),
            ("LightYellow", rgb(255, 255, 224)),
            ("yellow", rgb(255, 255, 0)),
            ("yellow3", rgb(205, 205, 0)),
            ("wheat", rgb(245, 222, 179)),
            ("wheat4", rgb(139, 126, 102)),
            ("royalblue", rgb(65, 105, 225)),
            ("royalblue3", rgb(58, 95, 205)),
            ("gray", rgb(190, 190, 190)),
            ("grey", rgb(190, 190, 190)),
            ("gray0", rgb(0, 0, 0)),
            ("grey0", rgb(0, 0, 0)),
            ("gray58", rgb(148, 148, 148)),
            ("grey58", rgb(148, 148, 148)),
            ("gray99", rgb(252, 252, 252)),
            ("grey99", rgb(252, 252, 252)),
        ] {
            assert_eq!(color_from_xterm_color(input), expected, "input={input}");
        }

        for invalid in [
            "",
            "r:",
            "rg:",
            "rgb:",
            "rgb:/",
            "rgb://",
            "rgb:///",
            "rgb:1",
            "rgb:1/",
            "rgb:/1",
            "rgb:1/1",
            "rgb:1/1/",
            "rgb:1/11/",
            "rgb:/1/1",
            "rgb:1/1/1/",
            "rgb:1/1/1/1",
            "rgb:111111111",
            "rgb:this/is/invalid",
            "rgba:1/1/1",
            "rgbi:1/1/1",
            "cmyk:1/1/1/1",
            "rgb#111",
            "rgb:#111",
            "rgb:rgb:1/1/1",
            "rgb:rgb:#111",
            "#",
            "#1",
            "#1111",
            "#11111",
            "#1/1/1",
            "#11/1/",
            "#1111111",
            "#/1/1/1",
            "#rgb:1/1/1",
            "#111invalid",
            "#invalid111",
            "#1111111111111111",
            "12/34/56",
            "123456",
            "rgb：1/1/1",
            "中文rgb:1/1/1",
            "rgb中文:1/1/1",
            "这是一句中文",
            "RGBİ1/1/1",
            "rgbİ1/1/1",
            "rgbİ:1/1/1",
            "rgß:1/1/1",
            "rgẞ:1/1/1",
            "yellow8",
            "yellow10",
            "yellow3a",
            "3yellow",
            "royal3blue",
            "5gray",
            "5gray8",
            "58grey",
            "gray-1",
            "gray101",
            "gray-",
            "gray;",
        ] {
            assert_eq!(color_from_xterm_color(invalid), None, "input={invalid}");
        }
    }

    #[test]
    fn microsoft_types_mangle_wsl_paths_contract() {
        let system32 = "C:\\Windows\\system32";
        let user_profile = "C:\\Users\\terminal";
        let check =
            |command: &str, directory: &str, expected_command: &str, expected_directory: &str| {
                let result =
                    mangle_starting_directory_for_wsl(command, directory, system32, user_profile);
                assert_eq!(result.command_line, expected_command, "command={command}");
                assert_eq!(
                    result.starting_directory, expected_directory,
                    "command={command}"
                );
            };

        check("wsl", "SENTINEL", "\"wsl\" --cd \"SENTINEL\" ", "");
        check("wsl -d X", "SENTINEL", "\"wsl\" --cd \"SENTINEL\" -d X", "");
        check(
            "wsl -d X ~/bin/sh",
            "SENTINEL",
            "\"wsl\" --cd \"SENTINEL\" -d X ~/bin/sh",
            "",
        );
        check("wsl.exe", "SENTINEL", "\"wsl.exe\" --cd \"SENTINEL\" ", "");
        check(
            "wsl.exe -d X",
            "SENTINEL",
            "\"wsl.exe\" --cd \"SENTINEL\" -d X",
            "",
        );
        check(
            "wsl.exe -d X ~/bin/sh",
            "SENTINEL",
            "\"wsl.exe\" --cd \"SENTINEL\" -d X ~/bin/sh",
            "",
        );
        check("\"wsl\"", "SENTINEL", "\"wsl\" --cd \"SENTINEL\" ", "");
        check(
            "\"wsl.exe\"",
            "SENTINEL",
            "\"wsl.exe\" --cd \"SENTINEL\" ",
            "",
        );
        check(
            "\"wsl\" -d X",
            "SENTINEL",
            "\"wsl\" --cd \"SENTINEL\"  -d X",
            "",
        );
        check(
            "\"wsl.exe\" -d X",
            "SENTINEL",
            "\"wsl.exe\" --cd \"SENTINEL\"  -d X",
            "",
        );
        check(
            "\"C:\\Windows\\system32\\wsl.exe\" -d X",
            "SENTINEL",
            "\"C:\\Windows\\system32\\wsl.exe\" --cd \"SENTINEL\"  -d X",
            "",
        );
        check(
            "\"C:\\windows\\system32\\wsl\" -d X",
            "SENTINEL",
            "\"C:\\windows\\system32\\wsl\" --cd \"SENTINEL\"  -d X",
            "",
        );
        check(
            "wsl ~/bin",
            "SENTINEL",
            "\"wsl\" --cd \"SENTINEL\" ~/bin",
            "",
        );

        check(
            "\"C:\\wsl.exe\" -d X",
            "SENTINEL",
            "\"C:\\wsl.exe\" -d X",
            "SENTINEL",
        );
        check("C:\\wsl.exe", "SENTINEL", "C:\\wsl.exe", "SENTINEL");
        check("wsl --cd C:\\", "SENTINEL", "wsl --cd C:\\", "SENTINEL");
        check("wsl ~", "SENTINEL", "wsl ~", "SENTINEL");
        check("wsl ~ -d Ubuntu", "SENTINEL", "wsl ~ -d Ubuntu", "SENTINEL");

        check(
            "wsl -d Ubuntu",
            "//wsl$/Ubuntu/home/user",
            "\"wsl\" --cd \"\\\\wsl$\\Ubuntu\\home\\user\" -d Ubuntu",
            "",
        );
        check(
            "wsl -d Ubuntu",
            "\\\\wsl$\\Ubuntu\\home\\user",
            "\"wsl\" --cd \"\\\\wsl$\\Ubuntu\\home\\user\" -d Ubuntu",
            "",
        );
        check(
            "wsl -d Ubuntu",
            "//wsl.localhost/Ubuntu/home/user",
            "\"wsl\" --cd \"\\\\wsl.localhost\\Ubuntu\\home\\user\" -d Ubuntu",
            "",
        );
        check(
            "wsl -d Ubuntu",
            "\\\\wsl.localhost\\Ubuntu\\home\\user",
            "\"wsl\" --cd \"\\\\wsl.localhost\\Ubuntu\\home\\user\" -d Ubuntu",
            "",
        );
        check("wsl -d Ubuntu", "~", "\"wsl\" --cd \"~\" -d Ubuntu", "");
        check("wsl ~ -d Ubuntu", "~", "wsl ~ -d Ubuntu", user_profile);
        check(
            "ubuntu ~ -d Ubuntu",
            "~",
            "ubuntu ~ -d Ubuntu",
            user_profile,
        );
        check("powershell.exe", "~", "powershell.exe", user_profile);
    }

    #[test]
    fn microsoft_types_trim_trailing_whitespace_contract() {
        for (input, expected) in [
            ("Foo   ", "Foo"),
            ("Foo\n", "Foo"),
            ("Foo\n\n", "Foo"),
            ("Foo\r\n", "Foo"),
            ("Foo Bar\n", "Foo Bar"),
            ("Foo\tBar\n", "Foo\tBar"),
            ("Foo Bar\t", "Foo Bar"),
            ("Foo Bar\t\t", "Foo Bar"),
            ("Foo Bar\t\n", "Foo Bar"),
            ("Foo\tBar\n\t", "Foo\tBar"),
        ] {
            assert_eq!(trim_paste(input), expected);
        }
    }

    #[test]
    fn microsoft_types_dont_trim_multiline_whitespace_contract() {
        for input in [
            "Foo\tBar",
            "Foo\nBar\n",
            "Foo  Baz\nBar\n",
            "Foo\tBaz\nBar\n",
            "Foo\tBaz\nBar\t\n",
        ] {
            assert_eq!(trim_paste(input), input);
        }
    }

    #[test]
    fn microsoft_types_evaluate_starting_directory_contract() {
        for cwd in ["C:\\Windows\\System32", "C:/Users/migrie"] {
            assert_eq!(evaluate_starting_directory(cwd, ""), format!("{cwd}\\"));
            assert_eq!(
                evaluate_starting_directory(cwd, "C:\\Windows"),
                "C:\\Windows"
            );
            assert_eq!(
                evaluate_starting_directory(cwd, "C:/Users/migrie"),
                "C:/Users/migrie"
            );
            assert_eq!(evaluate_starting_directory(cwd, "."), format!("{cwd}\\."));
            assert_eq!(
                evaluate_starting_directory(cwd, ".\\System32"),
                format!("{cwd}\\.\\System32")
            );
            assert_eq!(
                evaluate_starting_directory(cwd, "./dev"),
                format!("{cwd}\\./dev")
            );
            assert_eq!(evaluate_starting_directory(cwd, "~"), "~");
            assert_eq!(evaluate_starting_directory(cwd, "~/dev"), "~/dev");
            assert_eq!(evaluate_starting_directory(cwd, "/"), "/");
            assert_eq!(evaluate_starting_directory(cwd, "/dev"), "/dev");
        }
    }
}
