//! Interactive tester for the Kitty Keyboard Protocol.
//!
//! This tool allows you to toggle the 5 enhancement flags and see how key events
//! are encoded by the terminal emulator.
//!
//! Shortcuts:
//!   1-5: Toggle enhancement flags
//!   q/Ctrl+C: Quit

use std::fmt::Write as _;

// Enhancement flags
const FLAG_DISAMBIGUATE: u8 = 0b00001; // 1
const FLAG_EVENT_TYPES: u8 = 0b00010; // 2
const FLAG_ALTERNATE_KEYS: u8 = 0b00100; // 4
const FLAG_ALL_AS_ESCAPES: u8 = 0b01000; // 8
const FLAG_ASSOCIATED_TEXT: u8 = 0b10000; // 16

// Modifier bits (value is encoded as 1 + modifiers in the protocol)
const MOD_SHIFT: u8 = 0b00000001;
const MOD_ALT: u8 = 0b00000010;
const MOD_CTRL: u8 = 0b00000100;
const MOD_SUPER: u8 = 0b00001000;
const MOD_HYPER: u8 = 0b00010000;
const MOD_META: u8 = 0b00100000;
const MOD_CAPS_LOCK: u8 = 0b01000000;
const MOD_NUM_LOCK: u8 = 0b10000000;

fn main() {
    let mut terminal = Terminal::new().expect("Failed to initialize terminal");
    let mut output = String::with_capacity(4096);

    // Detect if terminal supports Kitty keyboard protocol
    // Send CSI ? u (query flags) followed by CSI c (DA1)
    terminal.write(b"\x1b[?u\x1b[c");

    let protocol_supported = detect_protocol_support(&mut terminal);

    let mut flags: u8 = 0;

    if protocol_supported {
        write_flags(&mut output, flags);
    } else {
        let _ = write!(
            output,
            "\x1b[1;33mNote:\x1b[m Terminal does not support Kitty keyboard protocol.\r\n"
        );
        let _ = write!(
            output,
            "      Key events will be shown in legacy format only.\r\n\r\n"
        );
    }
    write_help(&mut output, protocol_supported);
    terminal.write(output.as_bytes());
    output.clear();

    if protocol_supported {
        // Push initial flags (0) onto the stack
        write_push_keyboard_mode(&mut output, flags);
        terminal.write(output.as_bytes());
        output.clear();
    }

    let mut buf = [0u8; 64];
    loop {
        let n = terminal.read(&mut buf);
        if n == 0 {
            continue;
        }

        let input = &buf[..n];

        // Ctrl+C --> Exit
        if input == b"\x03" || input == b"\x1b[99;5u" {
            break;
        }

        if protocol_supported {
            let flag_to_toggle = match input {
                b"1" | b"\x1b[49u" | b"\x1b[49;;49u" => Some(FLAG_DISAMBIGUATE),
                b"2" | b"\x1b[50u" | b"\x1b[50;;50u" => Some(FLAG_EVENT_TYPES),
                b"3" | b"\x1b[51u" | b"\x1b[51;;51u" => Some(FLAG_ALTERNATE_KEYS),
                b"4" | b"\x1b[52u" | b"\x1b[52;;52u" => Some(FLAG_ALL_AS_ESCAPES),
                b"5" | b"\x1b[53u" | b"\x1b[53;;53u" => Some(FLAG_ASSOCIATED_TEXT),
                _ => None,
            };

            if let Some(flag) = flag_to_toggle {
                flags ^= flag;
                write_set_keyboard_mode(&mut output, flags);
                write_flags(&mut output, flags);
                write_help(&mut output, protocol_supported);
                terminal.write(output.as_bytes());
                output.clear();
                continue;
            }
        }

        write_decoded_input(&mut output, input);
        terminal.write(output.as_bytes());
        output.clear();
    }

    if protocol_supported {
        write_pop_keyboard_mode(&mut output);
        terminal.write(output.as_bytes());
    }
}

/// Detect Kitty keyboard protocol support by looking for CSI ? <num> u response
/// before the DA1 response (CSI ... c).
fn detect_protocol_support(terminal: &mut Terminal) -> bool {
    let mut buf = [0u8; 256];
    let mut response = Vec::new();
    let mut got_kitty_response = false;

    // Read until we see the DA1 response terminator 'c'
    loop {
        let n = terminal.read(&mut buf);
        response.extend_from_slice(&buf[..n]);

        // Parse the accumulated response
        let mut i = 0;
        while i < response.len() {
            if response[i] == 0x1b && i + 1 < response.len() && response[i + 1] == b'[' {
                // Found CSI, look for the terminator
                if let Some(end) = find_csi_end(&response[i + 2..]) {
                    let seq_end = i + 2 + end;
                    let params = &response[i + 2..seq_end];
                    let terminator = response[seq_end];

                    if terminator == b'u' && params.starts_with(b"?") {
                        // CSI ? <num> u - Kitty keyboard query response
                        got_kitty_response = true;
                    } else if terminator == b'c' {
                        // DA1 response - we're done
                        return got_kitty_response;
                    }

                    i = seq_end + 1;
                    continue;
                }
            }
            i += 1;
        }
    }
}

/// Find the end of CSI parameters (returns index of terminator byte)
fn find_csi_end(data: &[u8]) -> Option<usize> {
    for (i, &b) in data.iter().enumerate() {
        // CSI terminators are in the range 0x40-0x7E
        if (0x40..=0x7E).contains(&b) {
            return Some(i);
        }
        // Parameters and intermediates are in 0x20-0x3F range
        if !((0x20..=0x3F).contains(&b)) {
            return None;
        }
    }
    None
}

fn write_flags(out: &mut String, flags: u8) {
    let _ = write!(out, "\x1b[1mEnhancement Flags:\x1b[m\r\n");
    let _ = write!(
        out,
        "  [{}] \x1b[33m1\x1b[m: Disambiguate escape codes     (0b00001)\r\n",
        if flags & FLAG_DISAMBIGUATE != 0 {
            "\x1b[32m✓\x1b[m"
        } else {
            " "
        }
    );
    let _ = write!(
        out,
        "  [{}] \x1b[33m2\x1b[m: Report event types            (0b00010)\r\n",
        if flags & FLAG_EVENT_TYPES != 0 {
            "\x1b[32m✓\x1b[m"
        } else {
            " "
        }
    );
    let _ = write!(
        out,
        "  [{}] \x1b[33m3\x1b[m: Report alternate keys         (0b00100)\r\n",
        if flags & FLAG_ALTERNATE_KEYS != 0 {
            "\x1b[32m✓\x1b[m"
        } else {
            " "
        }
    );
    let _ = write!(
        out,
        "  [{}] \x1b[33m4\x1b[m: Report all keys as escapes    (0b01000)\r\n",
        if flags & FLAG_ALL_AS_ESCAPES != 0 {
            "\x1b[32m✓\x1b[m"
        } else {
            " "
        }
    );
    let _ = write!(
        out,
        "  [{}] \x1b[33m5\x1b[m: Report associated text        (0b10000)\r\n",
        if flags & FLAG_ASSOCIATED_TEXT != 0 {
            "\x1b[32m✓\x1b[m"
        } else {
            " "
        }
    );
    let _ = write!(out, "\r\n");
    let _ = write!(
        out,
        "  \x1b[1mCurrent flags value:\x1b[m \x1b[36m{}\x1b[m (0b{:05b})\r\n",
        flags, flags
    );
    let _ = write!(out, "\r\n");
}

fn write_help(out: &mut String, protocol_supported: bool) {
    let _ = write!(
        out,
        "\x1b[90m────────────────────────────────────────────────────────\x1b[m\r\n"
    );
    if protocol_supported {
        let _ = write!(out, "\x1b[1mControls:\x1b[m Press \x1b[33m1-5\x1b[m to toggle flags, \x1b[33mCtrl+C\x1b[m to quit\r\n");
    } else {
        let _ = write!(
            out,
            "\x1b[1mControls:\x1b[m Press \x1b[33mCtrl+C\x1b[m to quit\r\n"
        );
    }
    let _ = write!(
        out,
        "\x1b[90m────────────────────────────────────────────────────────\x1b[m\r\n"
    );
    let _ = write!(out, "\r\n");
    let _ = write!(out, "\x1b[1mKey events:\x1b[m\r\n");
    let _ = write!(out, "\r\n");
}

fn write_push_keyboard_mode(out: &mut String, flags: u8) {
    // CSI > flags u - Push flags onto the stack
    let _ = write!(out, "\x1b[>{}u", flags);
}

fn write_set_keyboard_mode(out: &mut String, flags: u8) {
    // CSI = flags ; 1 u - Set flags (mode 1 = replace all)
    let _ = write!(out, "\x1b[={};1u", flags);
}

fn write_pop_keyboard_mode(out: &mut String) {
    // CSI < u - Pop from the stack (restores previous mode)
    let _ = write!(out, "\x1b[<u");
}

fn write_decoded_input(out: &mut String, input: &[u8]) {
    let _ = write!(out, "\x1b[37m\"");

    // Print as escaped string
    for &b in input {
        match b {
            0x1b => {
                let _ = write!(out, "\\x1b");
            }
            0x00..=0x1f => {
                let _ = write!(out, "\\x{:02x}", b);
            }
            0x7f => {
                let _ = write!(out, "\\x7f");
            }
            _ => {
                let _ = write!(out, "{}", b as char);
            }
        }
    }
    let _ = write!(out, "\"");

    // Try to decode as Kitty protocol
    if let Some(decoded) = decode_kitty_sequence(input) {
        let _ = write!(out, "\x1b[m\r\n      \x1b[1;32m→ {}\x1b[m", decoded);
    } else if let Some(decoded) = decode_legacy_sequence(input) {
        let _ = write!(
            out,
            "\x1b[m\r\n      \x1b[1;33m→ {} (legacy)\x1b[m",
            decoded
        );
    } else if input.len() == 1 && input[0] >= 0x20 && input[0] < 0x7f {
        let _ = write!(
            out,
            "\x1b[m\r\n      \x1b[1;34m→ Character: '{}'\x1b[m",
            input[0] as char
        );
    } else if input.len() == 1 {
        if let Some(name) = control_char_name(input[0]) {
            let _ = write!(out, "\x1b[m\r\n      \x1b[1;35m→ {}\x1b[m", name);
        }
    }

    let _ = write!(out, "\x1b[m\r\n");
}

fn decode_kitty_sequence(input: &[u8]) -> Option<String> {
    // Check for CSI ... u format
    if input.len() < 3 || input[0] != 0x1b || input[1] != b'[' {
        return None;
    }

    let rest = &input[2..];

    // CSI ? flags u - Query response
    if rest.starts_with(b"?") && rest.ends_with(b"u") {
        let num_str = std::str::from_utf8(&rest[1..rest.len() - 1]).ok()?;
        if let Ok(flags) = num_str.parse::<u8>() {
            return Some(format!("Query response: flags={} (0b{:05b})", flags, flags));
        }
    }

    // CSI ... u - Key event
    if rest.ends_with(b"u") {
        return decode_csi_u_sequence(&rest[..rest.len() - 1]);
    }

    // CSI ... ~ - Legacy functional key with possible kitty extensions
    if rest.ends_with(b"~") {
        return decode_csi_tilde_sequence(&rest[..rest.len() - 1]);
    }

    None
}

/// Parse "modifiers:event_type" parameter, returns (modifiers, event_type)
fn parse_modifiers_and_event(mod_part: Option<&str>) -> (u8, u8) {
    if let Some(mod_part) = mod_part {
        let mut mod_parts = mod_part.split(':');
        let mods: u8 = mod_parts
            .next()
            .and_then(|s| s.parse::<u8>().ok())
            .unwrap_or(1)
            .saturating_sub(1);
        let evt: u8 = mod_parts.next().and_then(|s| s.parse().ok()).unwrap_or(1);
        (mods, evt)
    } else {
        (0, 1)
    }
}

/// Format a codepoint as a readable key name
fn format_codepoint(code: u32) -> String {
    if let Some(c) = char::from_u32(code) {
        if c.is_control() {
            format!("{}", code)
        } else {
            format!("'{}' ({})", c, code)
        }
    } else {
        format!("{}", code)
    }
}

fn decode_csi_u_sequence(params: &[u8]) -> Option<String> {
    let params_str = std::str::from_utf8(params).ok()?;
    let mut parts = params_str.split(';');

    // Parse key code (may have alternate keys separated by colons)
    let key_part = parts.next()?;
    let mut key_codes = key_part.split(':');
    let key_code: u32 = key_codes.next()?.parse().ok()?;
    let shifted_key: Option<u32> =
        key_codes
            .next()
            .and_then(|s| if s.is_empty() { None } else { s.parse().ok() });
    let base_layout_key: Option<u32> = key_codes.next().and_then(|s| s.parse().ok());

    let (modifiers, event_type) = parse_modifiers_and_event(parts.next());

    // Parse associated text
    let text_codepoints: Option<String> = parts.next().map(|text_part| {
        text_part
            .split(':')
            .filter_map(|s| s.parse::<u32>().ok())
            .filter_map(char::from_u32)
            .collect()
    });

    // Build result
    let key_name = key_code_to_name(key_code);
    let event_name = match event_type {
        1 => "press",
        2 => "repeat",
        3 => "release",
        _ => "unknown",
    };

    let mut result = format!("Key: {} ({}), Event: {}", key_name, key_code, event_name);

    if modifiers != 0 {
        result.push_str(&format!(", Modifiers: {}", modifiers_to_string(modifiers)));
    }

    if let Some(shifted) = shifted_key {
        result.push_str(&format!(", Shifted: {}", format_codepoint(shifted)));
    }

    if let Some(base) = base_layout_key {
        result.push_str(&format!(", Base layout: {}", format_codepoint(base)));
    }

    if let Some(text) = text_codepoints {
        if !text.is_empty() {
            result.push_str(&format!(", Text: \"{}\"", text));
        }
    }

    Some(result)
}

fn decode_csi_tilde_sequence(params: &[u8]) -> Option<String> {
    let params_str = std::str::from_utf8(params).ok()?;
    let mut parts = params_str.split(';');

    let key_num: u32 = parts.next()?.parse().ok()?;
    let (modifiers, event_type) = parse_modifiers_and_event(parts.next());

    let key_name = match key_num {
        2 => "Insert",
        3 => "Delete",
        5 => "PageUp",
        6 => "PageDown",
        7 => "Home",
        8 => "End",
        11 => "F1",
        12 => "F2",
        13 => "F3",
        14 => "F4",
        15 => "F5",
        17 => "F6",
        18 => "F7",
        19 => "F8",
        20 => "F9",
        21 => "F10",
        23 => "F11",
        24 => "F12",
        29 => "Menu",
        _ => return Some(format!("Unknown functional key: {}", key_num)),
    };

    let event_name = match event_type {
        1 => "press",
        2 => "repeat",
        3 => "release",
        _ => "unknown",
    };

    let mut result = format!("Key: {}, Event: {}", key_name, event_name);
    if modifiers != 0 {
        result.push_str(&format!(", Modifiers: {}", modifiers_to_string(modifiers)));
    }

    Some(result)
}

fn decode_legacy_sequence(input: &[u8]) -> Option<String> {
    if input.len() < 2 || input[0] != 0x1b {
        return None;
    }

    // SS3 sequences (ESC O ...)
    if input.len() >= 3 && input[1] == b'O' {
        let key = match input[2] {
            b'A' => "Up",
            b'B' => "Down",
            b'C' => "Right",
            b'D' => "Left",
            b'H' => "Home",
            b'F' => "End",
            b'P' => "F1",
            b'Q' => "F2",
            b'R' => "F3",
            b'S' => "F4",
            _ => return None,
        };
        return Some(format!("Key: {} (SS3)", key));
    }

    // CSI sequences
    if input.len() >= 3 && input[1] == b'[' {
        let rest = &input[2..];

        // CSI letter - simple cursor keys
        if rest.len() == 1 {
            let key = match rest[0] {
                b'A' => "Up",
                b'B' => "Down",
                b'C' => "Right",
                b'D' => "Left",
                b'H' => "Home",
                b'F' => "End",
                b'Z' => "Shift+Tab",
                _ => return None,
            };
            return Some(format!("Key: {}", key));
        }

        // CSI 1 ; modifier letter
        if rest.len() >= 4 && rest[0] == b'1' && rest[1] == b';' {
            if let Ok(mod_str) = std::str::from_utf8(&rest[2..rest.len() - 1]) {
                if let Ok(mod_val) = mod_str.parse::<u8>() {
                    let modifiers = mod_val.saturating_sub(1);
                    let key = match rest[rest.len() - 1] {
                        b'A' => "Up",
                        b'B' => "Down",
                        b'C' => "Right",
                        b'D' => "Left",
                        b'H' => "Home",
                        b'F' => "End",
                        b'P' => "F1",
                        b'Q' => "F2",
                        b'S' => "F4",
                        _ => return None,
                    };
                    return Some(format!(
                        "Key: {}, Modifiers: {}",
                        key,
                        modifiers_to_string(modifiers)
                    ));
                }
            }
        }
    }

    // Alt + key
    if input.len() == 2 && input[1] >= 0x20 && input[1] < 0x7f {
        return Some(format!("Alt+'{}'", input[1] as char));
    }

    None
}

fn key_code_to_name(code: u32) -> String {
    match code {
        9 => "Tab".to_string(),
        13 => "Enter".to_string(),
        27 => "Escape".to_string(),
        32 => "Space".to_string(),
        127 => "Backspace".to_string(),

        // Functional keys in Private Use Area
        57358 => "CapsLock".to_string(),
        57359 => "ScrollLock".to_string(),
        57360 => "NumLock".to_string(),
        57361 => "PrintScreen".to_string(),
        57362 => "Pause".to_string(),
        57363 => "Menu".to_string(),

        57376..=57398 => format!("F{}", code - 57376 + 13), // F13-F35

        57399..=57408 => format!("KP_{}", code - 57399), // KP_0 - KP_9
        57409 => "KP_Decimal".to_string(),
        57410 => "KP_Divide".to_string(),
        57411 => "KP_Multiply".to_string(),
        57412 => "KP_Subtract".to_string(),
        57413 => "KP_Add".to_string(),
        57414 => "KP_Enter".to_string(),
        57415 => "KP_Equal".to_string(),
        57416 => "KP_Separator".to_string(),
        57417 => "KP_Left".to_string(),
        57418 => "KP_Right".to_string(),
        57419 => "KP_Up".to_string(),
        57420 => "KP_Down".to_string(),
        57421 => "KP_PageUp".to_string(),
        57422 => "KP_PageDown".to_string(),
        57423 => "KP_Home".to_string(),
        57424 => "KP_End".to_string(),
        57425 => "KP_Insert".to_string(),
        57426 => "KP_Delete".to_string(),
        57427 => "KP_Begin".to_string(),

        57428 => "MediaPlay".to_string(),
        57429 => "MediaPause".to_string(),
        57430 => "MediaPlayPause".to_string(),
        57431 => "MediaReverse".to_string(),
        57432 => "MediaStop".to_string(),
        57433 => "MediaFastForward".to_string(),
        57434 => "MediaRewind".to_string(),
        57435 => "MediaTrackNext".to_string(),
        57436 => "MediaTrackPrevious".to_string(),
        57437 => "MediaRecord".to_string(),
        57438 => "LowerVolume".to_string(),
        57439 => "RaiseVolume".to_string(),
        57440 => "MuteVolume".to_string(),

        57441 => "LeftShift".to_string(),
        57442 => "LeftControl".to_string(),
        57443 => "LeftAlt".to_string(),
        57444 => "LeftSuper".to_string(),
        57445 => "LeftHyper".to_string(),
        57446 => "LeftMeta".to_string(),
        57447 => "RightShift".to_string(),
        57448 => "RightControl".to_string(),
        57449 => "RightAlt".to_string(),
        57450 => "RightSuper".to_string(),
        57451 => "RightHyper".to_string(),
        57452 => "RightMeta".to_string(),
        57453 => "IsoLevel3Shift".to_string(),
        57454 => "IsoLevel5Shift".to_string(),

        // Regular characters
        c if (0x20..0x7f).contains(&c) => format!("'{}'", char::from_u32(c).unwrap()),
        c => {
            if let Some(ch) = char::from_u32(c) {
                format!("'{}' (U+{:04X})", ch, c)
            } else {
                format!("U+{:04X}", c)
            }
        }
    }
}

fn modifiers_to_string(mods: u8) -> String {
    let mut parts = Vec::new();
    if mods & MOD_SHIFT != 0 {
        parts.push("Shift");
    }
    if mods & MOD_ALT != 0 {
        parts.push("Alt");
    }
    if mods & MOD_CTRL != 0 {
        parts.push("Ctrl");
    }
    if mods & MOD_SUPER != 0 {
        parts.push("Super");
    }
    if mods & MOD_HYPER != 0 {
        parts.push("Hyper");
    }
    if mods & MOD_META != 0 {
        parts.push("Meta");
    }
    if mods & MOD_CAPS_LOCK != 0 {
        parts.push("CapsLock");
    }
    if mods & MOD_NUM_LOCK != 0 {
        parts.push("NumLock");
    }
    if parts.is_empty() {
        "None".to_string()
    } else {
        parts.join("+")
    }
}

fn control_char_name(b: u8) -> Option<&'static str> {
    match b {
        0x00 => Some("Ctrl+Space (NUL)"),
        0x01 => Some("Ctrl+A"),
        0x02 => Some("Ctrl+B"),
        0x03 => Some("Ctrl+C"),
        0x04 => Some("Ctrl+D"),
        0x05 => Some("Ctrl+E"),
        0x06 => Some("Ctrl+F"),
        0x07 => Some("Ctrl+G (BEL)"),
        0x08 => Some("Ctrl+H (Backspace)"),
        0x09 => Some("Tab"),
        0x0a => Some("Ctrl+J (Line Feed)"),
        0x0b => Some("Ctrl+K"),
        0x0c => Some("Ctrl+L"),
        0x0d => Some("Enter"),
        0x0e => Some("Ctrl+N"),
        0x0f => Some("Ctrl+O"),
        0x10 => Some("Ctrl+P"),
        0x11 => Some("Ctrl+Q"),
        0x12 => Some("Ctrl+R"),
        0x13 => Some("Ctrl+S"),
        0x14 => Some("Ctrl+T"),
        0x15 => Some("Ctrl+U"),
        0x16 => Some("Ctrl+V"),
        0x17 => Some("Ctrl+W"),
        0x18 => Some("Ctrl+X"),
        0x19 => Some("Ctrl+Y"),
        0x1a => Some("Ctrl+Z"),
        0x1b => Some("Escape"),
        0x1c => Some("Ctrl+\\"),
        0x1d => Some("Ctrl+]"),
        0x1e => Some("Ctrl+^"),
        0x1f => Some("Ctrl+_"),
        0x7f => Some("Backspace (DEL)"),
        _ => None,
    }
}

// Platform-specific terminal handling

#[cfg(unix)]
mod platform {
    use std::io;
    use std::mem::MaybeUninit;

    const STDIN_FILENO: libc::c_int = 0;
    const STDOUT_FILENO: libc::c_int = 1;

    pub struct Terminal {
        original_termios: libc::termios,
    }

    impl Terminal {
        pub fn new() -> io::Result<Self> {
            let mut termios = MaybeUninit::uninit();

            unsafe {
                if libc::tcgetattr(STDIN_FILENO, termios.as_mut_ptr()) != 0 {
                    return Err(io::Error::last_os_error());
                }
            }

            let original_termios = unsafe { termios.assume_init() };
            let mut raw = original_termios;

            // Set raw mode
            raw.c_lflag &= !(libc::ECHO | libc::ICANON | libc::ISIG | libc::IEXTEN);
            raw.c_iflag &= !(libc::IXON | libc::ICRNL | libc::BRKINT | libc::INPCK | libc::ISTRIP);
            raw.c_oflag &= !libc::OPOST;

            unsafe {
                if libc::tcsetattr(STDIN_FILENO, libc::TCSAFLUSH, &raw) != 0 {
                    return Err(io::Error::last_os_error());
                }
            }

            Ok(Terminal { original_termios })
        }

        pub fn read(&mut self, buf: &mut [u8]) -> usize {
            unsafe {
                let n = libc::read(
                    STDIN_FILENO,
                    buf.as_mut_ptr() as *mut libc::c_void,
                    buf.len(),
                );
                if n < 0 {
                    0
                } else {
                    n as usize
                }
            }
        }

        pub fn write(&mut self, buf: &[u8]) {
            unsafe {
                libc::write(
                    STDOUT_FILENO,
                    buf.as_ptr() as *const libc::c_void,
                    buf.len(),
                );
            }
        }
    }

    impl Drop for Terminal {
        fn drop(&mut self) {
            unsafe {
                libc::tcsetattr(STDIN_FILENO, libc::TCSAFLUSH, &self.original_termios);
            }
        }
    }
}

#[allow(non_camel_case_types, clippy::upper_case_acronyms)]
#[cfg(windows)]
mod platform {
    use std::io;

    type BOOL = i32;
    type HANDLE = *mut core::ffi::c_void;
    type CONSOLE_MODE = u32;
    type STD_HANDLE = u32;

    const STD_INPUT_HANDLE: STD_HANDLE = 0xFFFFFFF6;
    const STD_OUTPUT_HANDLE: STD_HANDLE = 0xFFFFFFF5;
    const ENABLE_PROCESSED_OUTPUT: CONSOLE_MODE = 1u32;
    const ENABLE_WRAP_AT_EOL_OUTPUT: CONSOLE_MODE = 2u32;
    const ENABLE_VIRTUAL_TERMINAL_PROCESSING: CONSOLE_MODE = 4u32;
    const DISABLE_NEWLINE_AUTO_RETURN: CONSOLE_MODE = 8u32;
    const ENABLE_VIRTUAL_TERMINAL_INPUT: CONSOLE_MODE = 512u32;
    const CP_UTF8: u32 = 65001;

    unsafe extern "system" {
        fn ReadFile(
            hfile: HANDLE,
            lpbuffer: *mut u8,
            nnumberofbytestoread: u32,
            lpnumberofbytesread: *mut u32,
            lpoverlapped: *mut (),
        ) -> BOOL;

        fn WriteFile(
            hfile: HANDLE,
            lpbuffer: *const u8,
            nnumberofbytestowrite: u32,
            lpnumberofbyteswritten: *mut u32,
            lpoverlapped: *mut (),
        ) -> BOOL;

        fn GetStdHandle(nstdhandle: STD_HANDLE) -> HANDLE;
        fn GetConsoleMode(hconsolehandle: HANDLE, lpmode: *mut CONSOLE_MODE) -> BOOL;
        fn SetConsoleMode(hconsolehandle: HANDLE, dwmode: CONSOLE_MODE) -> BOOL;
        fn GetConsoleCP() -> u32;
        fn SetConsoleCP(wcodepageid: u32) -> BOOL;
        fn GetConsoleOutputCP() -> u32;
        fn SetConsoleOutputCP(wcodepageid: u32) -> BOOL;
    }

    pub struct Terminal {
        stdin_handle: HANDLE,
        stdout_handle: HANDLE,
        stdin_mode: CONSOLE_MODE,
        stdout_mode: CONSOLE_MODE,
        stdin_cp: u32,
        stdout_cp: u32,
    }

    impl Terminal {
        pub fn new() -> io::Result<Self> {
            unsafe {
                let stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
                let stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

                let mut stdin_mode: CONSOLE_MODE = 0;
                let mut stdout_mode: CONSOLE_MODE = 0;
                GetConsoleMode(stdin_handle, &mut stdin_mode);
                GetConsoleMode(stdin_handle, &mut stdout_mode);

                SetConsoleMode(stdin_handle, ENABLE_VIRTUAL_TERMINAL_INPUT);
                SetConsoleMode(
                    stdout_handle,
                    ENABLE_PROCESSED_OUTPUT
                        | ENABLE_WRAP_AT_EOL_OUTPUT
                        | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                        | DISABLE_NEWLINE_AUTO_RETURN,
                );

                let stdin_cp = GetConsoleCP();
                let stdout_cp = GetConsoleOutputCP();
                SetConsoleCP(CP_UTF8);
                SetConsoleOutputCP(CP_UTF8);

                Ok(Terminal {
                    stdin_handle,
                    stdout_handle,
                    stdin_mode,
                    stdout_mode,
                    stdin_cp,
                    stdout_cp,
                })
            }
        }

        pub fn read(&mut self, buf: &mut [u8]) -> usize {
            unsafe {
                let mut bytes_read: u32 = 0;
                if ReadFile(
                    self.stdin_handle,
                    buf.as_mut_ptr() as *mut _,
                    buf.len() as u32,
                    &mut bytes_read,
                    std::ptr::null_mut(),
                ) == 0
                {
                    0
                } else {
                    bytes_read as usize
                }
            }
        }

        pub fn write(&mut self, buf: &[u8]) {
            unsafe {
                let mut bytes_written: u32 = 0;
                WriteFile(
                    self.stdout_handle,
                    buf.as_ptr() as *const _,
                    buf.len() as u32,
                    &mut bytes_written,
                    std::ptr::null_mut(),
                );
            }
        }
    }

    impl Drop for Terminal {
        fn drop(&mut self) {
            unsafe {
                SetConsoleMode(self.stdin_handle, self.stdin_mode);
                SetConsoleMode(self.stdout_handle, self.stdout_mode);
                SetConsoleCP(self.stdin_cp);
                SetConsoleOutputCP(self.stdout_cp);
            }
        }
    }
}

use platform::Terminal;
