# Windows Terminal Color Names and ECMA-48 Relation

## Introduction

This document provides a comprehensive guide to understanding the relationship between Windows Terminal color names and the ECMA-48 standard. Windows Terminal uses a sophisticated color naming system that closely aligns with the ECMA-48 (also known as ANSI escape codes) specification for terminal control sequences.

The ECMA-48 standard defines control functions for coded character sets, including color manipulation in terminal applications. Windows Terminal implements these standards while also providing user-friendly color names that make configuration more intuitive for both beginners and advanced users.

## What is ECMA-48?

ECMA-48, also published as ISO/IEC 6429, is an international standard that defines control functions for coded character sets. In the context of terminal applications, it primarily governs:

- Text formatting (bold, italic, underline)
- Cursor positioning and movement
- Screen clearing and scrolling
- **Color control sequences** (SGR - Select Graphic Rendition)

The standard ensures consistency across different terminal implementations, allowing applications to produce predictable visual output regardless of the specific terminal being used.

## ANSI Color Table

Windows Terminal supports both the standard 8-color and extended 16-color ANSI palette, plus 256-color and 24-bit true color modes. Here's the comprehensive breakdown:

### Standard 8 Colors (3-bit)

| Color Name | ANSI Code | RGB (Default) | Windows Terminal Name |
|------------|-----------|---------------|----------------------|
| Black      | `\033[30m` | #000000      | `black`              |
| Red        | `\033[31m` | #800000      | `red`                |
| Green      | `\033[32m` | #008000      | `green`              |
| Yellow     | `\033[33m` | #808000      | `yellow`             |
| Blue       | `\033[34m` | #000080      | `blue`               |
| Magenta    | `\033[35m` | #800080      | `magenta`            |
| Cyan       | `\033[36m` | #008080      | `cyan`               |
| White      | `\033[37m` | #C0C0C0      | `white`              |

### Bright Colors (4-bit extended)

| Color Name    | ANSI Code | RGB (Default) | Windows Terminal Name |
|---------------|-----------|---------------|----------------------|
| Bright Black  | `\033[90m` | #808080      | `brightBlack`        |
| Bright Red    | `\033[91m` | #FF0000      | `brightRed`          |
| Bright Green  | `\033[92m` | #00FF00      | `brightGreen`        |
| Bright Yellow | `\033[93m` | #FFFF00      | `brightYellow`       |
| Bright Blue   | `\033[94m` | #0000FF      | `brightBlue`         |
| Bright Magenta| `\033[95m` | #FF00FF      | `brightMagenta`      |
| Bright Cyan   | `\033[96m` | #00FFFF      | `brightCyan`         |
| Bright White  | `\033[97m` | #FFFFFF      | `brightWhite`        |

### Background Colors

Background colors use the same naming convention with ANSI codes 40-47 for standard colors and 100-107 for bright colors:

- Background Black: `\033[40m`
- Background Red: `\033[41m`
- And so on...

### 256-Color Mode

Windows Terminal supports 256-color mode using the sequence `\033[38;5;Nm` for foreground and `\033[48;5;Nm` for background, where N is 0-255.

### True Color (24-bit)

For maximum color fidelity, Windows Terminal supports 24-bit true color using:
- Foreground: `\033[38;2;R;G;Bm`
- Background: `\033[48;2;R;G;Bm`

Where R, G, B are values from 0-255.

## Windows Terminal Color Configuration

Windows Terminal allows customization of these colors through its settings.json file or GUI settings. Each color scheme can define custom RGB values for all 16 basic colors:

```json
{
    "name": "Campbell",
    "black": "#0C0C0C",
    "red": "#C50F1F",
    "green": "#13A10E",
    "yellow": "#C19C00",
    "blue": "#0037DA",
    "purple": "#881798",
    "cyan": "#3A96DD",
    "white": "#CCCCCC",
    "brightBlack": "#767676",
    "brightRed": "#E74856",
    "brightGreen": "#16C60C",
    "brightYellow": "#F9F1A5",
    "brightBlue": "#3B78FF",
    "brightPurple": "#B4009E",
    "brightCyan": "#61D6D6",
    "brightWhite": "#F2F2F2",
    "background": "#012456",
    "foreground": "#CCCCCC"
}
```

## Practical Examples

### Basic Usage

1. **Setting text color to red:**
   ```bash
   echo -e "\033[31mThis text is red\033[0m"
   ```

2. **Setting background to blue:**
   ```bash
   echo -e "\033[44mThis has a blue background\033[0m"
   ```

3. **Combining foreground and background:**
   ```bash
   echo -e "\033[33;44mYellow text on blue background\033[0m"
   ```

### PowerShell Examples

```powershell
# Using Write-Host with color names
Write-Host "Hello World" -ForegroundColor Red -BackgroundColor Yellow

# Using ANSI escape sequences
Write-Host "`e[31mRed text`e[0m"
```

## Beginner's Guide to Terminal Colors

### Understanding the Basics

If you're new to terminal colors, here are the key concepts to understand:

1. **ANSI Escape Codes**: Special sequences that start with `\033[` (or `\e[`) that tell the terminal to change appearance.

2. **SGR (Select Graphic Rendition)**: The specific type of ANSI code used for colors and formatting.

3. **Color Reset**: Always use `\033[0m` to reset colors to default after setting them.

### Step-by-Step Color Usage

1. **Start with basic colors**: Learn the 8 standard colors first
2. **Practice with examples**: Use simple echo commands to see colors
3. **Understand color schemes**: Learn how Windows Terminal color schemes work
4. **Customize gradually**: Start with small modifications to existing schemes

### Common Pitfalls to Avoid

- **Forgetting to reset colors**: Always end color sequences with `\033[0m`
- **Mixing incompatible modes**: Don't mix 8-bit and 24-bit color codes
- **Ignoring accessibility**: Consider users with color vision deficiencies
- **Overusing colors**: Too many colors can make text hard to read

## Advanced Features

### Color Schemes

Windows Terminal supports multiple color schemes that can be switched dynamically:

- Campbell (default)
- Campbell Powershell
- One Half Dark/Light
- Solarized Dark/Light
- Tango Dark/Light
- And many more...

### Dynamic Color Changes

Applications can request terminal color information and even modify the color palette at runtime using specific ECMA-48 sequences.

### Compatibility Considerations

While Windows Terminal has excellent ECMA-48 support, some legacy applications might expect different behavior. Windows Terminal includes compatibility modes for:

- Legacy console applications
- UNIX/Linux terminal applications
- PowerShell ISE compatibility

## Summary

Windows Terminal's color system provides a modern implementation of the ECMA-48 standard while maintaining backward compatibility with legacy applications. Key takeaways:

1. **Standards Compliance**: Windows Terminal closely follows ECMA-48/ANSI standards
2. **Flexibility**: Supports 8-color, 16-color, 256-color, and 24-bit true color modes
3. **Customization**: Full control over color schemes through settings.json
4. **User-Friendly**: Provides named colors alongside numeric codes
5. **Accessibility**: Supports high contrast and customizable themes

For developers and power users, understanding this relationship enables:
- Better application design with predictable color output
- Effective customization of terminal appearance
- Improved accessibility through thoughtful color choices
- Cross-platform compatibility when using standard ANSI codes

## Additional Resources

- [ECMA-48 Standard Document](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [Windows Terminal Documentation](https://docs.microsoft.com/en-us/windows/terminal/)
- [ANSI Escape Code Reference](https://en.wikipedia.org/wiki/ANSI_escape_code)
- [Color Scheme Gallery](https://windowsterminalthemes.dev/)

---

*This document serves as a comprehensive guide to Windows Terminal color implementation and its adherence to international standards. For the most up-to-date information, always refer to the official Windows Terminal documentation.*
