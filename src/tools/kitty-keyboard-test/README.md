# Kitty Keyboard Protocol Tester

An interactive tool for testing the [Kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) enhancement flags.

## Building

```sh
cargo build --release
```

## Usage

Run the tool in a terminal that supports the Kitty keyboard protocol:

```sh
cargo run
```

or after building:

```sh
./target/release/kitty-keyboard-test
```

## Controls

| Key | Action |
|-----|--------|
| `1` | Toggle **Disambiguate escape codes** (0b00001) |
| `2` | Toggle **Report event types** (0b00010) |
| `3` | Toggle **Report alternate keys** (0b00100) |
| `4` | Toggle **Report all keys as escape codes** (0b01000) |
| `5` | Toggle **Report associated text** (0b10000) |
| `q` or `Ctrl+C` | Quit |

## Enhancement Flags

1. **Disambiguate escape codes** (bit 0, value 1): Fixes legacy escape code ambiguities. Keys like Esc, Alt+key, Ctrl+key are reported using CSI u sequences.

2. **Report event types** (bit 1, value 2): Reports key press, repeat, and release events. Without this flag, only press events are reported.

3. **Report alternate keys** (bit 2, value 4): Reports shifted key and base layout key in addition to the main key code. Useful for shortcut matching across keyboard layouts.

4. **Report all keys as escape codes** (bit 3, value 8): Even text-producing keys (like regular letters) are reported as escape codes instead of plain text. Required for games and applications that need key events for all keys.

5. **Report associated text** (bit 4, value 16): When used with flag 4, also reports the text that the key would produce. The text is encoded as Unicode codepoints in the escape sequence.

## Output Format

For each key event, the tool displays:
- **Raw bytes**: The actual bytes received from the terminal
- **Escaped string**: A readable representation of the bytes
- **Decoded event**: Human-readable interpretation including key name, modifiers, event type, and any alternate keys or associated text

## Example Output

```
Raw: [0x1b, 0x5b, 0x97, 0x3b, 0x32, 0x3b, 0x41, 0x75]  Str: "\x1b[97;2;65u"
      → Key: 'a' (97), Event: press, Modifiers: Shift, Text: "A"
```

## Protocol Reference

- Escape sequence to push keyboard mode: `CSI > flags u`
- Escape sequence to pop keyboard mode: `CSI < u`
- Key event format: `CSI keycode:shifted:base ; modifiers:event ; text u`

Modifiers are encoded as `1 + modifier_bits`:
- Shift: bit 0 (1)
- Alt: bit 1 (2)
- Ctrl: bit 2 (4)
- Super: bit 3 (8)
- Hyper: bit 4 (16)
- Meta: bit 5 (32)
- CapsLock: bit 6 (64)
- NumLock: bit 7 (128)

Event types:
- Press: 1 (default if omitted)
- Repeat: 2
- Release: 3
