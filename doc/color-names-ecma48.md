# Windows Terminal Color Names: Understanding ECMA-48 Standards

## Introduction: Why Color Names Can Be Confusing

Colors in terminal applications often confuse users because different systems use different naming conventions, numbering schemes, and standards. You might see references to:

- **ANSI colors** (the most common term)
- **ECMA-48 colors** (the official standard)
- **ISO/IEC 6429** (another name for the same standard)
- **VT100 colors** (named after the famous terminal)
- **16-color palette**, **256-color palette**, or **true color**
- Confusing names like "bright black" (which is actually gray)

This document clarifies these terms and explains how Windows Terminal implements the **ECMA-48 standard** for color control, making it easier for both beginners and experienced users to understand and use terminal colors effectively.

### What This Document Covers

This guide provides:
1. **Clear explanations** of confusing color terminology
2. **Simple ANSI/ECMA-48 mapping tables** for easy reference  
3. **Plain-language notes** for better understanding
4. **Practical examples** and code references
5. **Links to additional resources**

---

## What is ECMA-48?

**ECMA-48** (also published as **ISO/IEC 6429**) is the international standard that defines control functions for coded character sets. When people say "ANSI colors," they're usually referring to this standard.

### Key Concepts:
- **SGR (Select Graphic Rendition)**: The specific part of ECMA-48 that controls colors and text formatting
- **Escape sequences**: Special codes that start with `\033[` (or `\e[`) that tell the terminal to change appearance
- **Control functions**: Commands for cursor movement, screen clearing, and color changes

### Why It Matters:
The standard ensures that color codes work consistently across different terminal applications, whether you're using Windows Terminal, macOS Terminal, or Linux terminals.

---

## ANSI/ECMA-48 Color Mapping Table

### Standard 8 Colors (3-bit color)

These are the foundation colors that all terminals support:

| Color Name | ANSI Code | Typical RGB | Windows Terminal Name | Common Use |
|------------|-----------|-------------|----------------------|------------|
| Black      | `\033[30m` | `#000000`   | `"black"`             | Normal text background |
| Red        | `\033[31m` | `#800000`   | `"red"`               | Errors, warnings |
| Green      | `\033[32m` | `#008000`   | `"green"`             | Success messages |
| Yellow     | `\033[33m` | `#808000`   | `"yellow"`            | Warnings, highlights |
| Blue       | `\033[34m` | `#000080`   | `"blue"`              | Information, links |
| Magenta    | `\033[35m` | `#800080`   | `"purple"`            | Special elements |
| Cyan       | `\033[36m` | `#008080`   | `"cyan"`              | Metadata, secondary info |
| White      | `\033[37m` | `#C0C0C0`   | `"white"`             | Normal text |

### Bright Colors (4-bit color extension)

**Note**: "Bright black" is actually gray - this naming often confuses beginners!

| Color Name     | ANSI Code | Typical RGB | Windows Terminal Name | Plain Language |
|----------------|-----------|-------------|----------------------|----------------|
| Bright Black   | `\033[90m` | `#808080`   | `"brightBlack"`       | Gray |
| Bright Red     | `\033[91m` | `#FF0000`   | `"brightRed"`         | Vivid red |
| Bright Green   | `\033[92m` | `#00FF00`   | `"brightGreen"`       | Vivid green |
| Bright Yellow  | `\033[93m` | `#FFFF00`   | `"brightYellow"`      | Vivid yellow |
| Bright Blue    | `\033[94m` | `#0000FF`   | `"brightBlue"`        | Vivid blue |
| Bright Magenta | `\033[95m` | `#FF00FF`   | `"brightPurple"`      | Vivid purple |
| Bright Cyan    | `\033[96m` | `#00FFFF`   | `"brightCyan"`        | Vivid cyan |
| Bright White   | `\033[97m` | `#FFFFFF`   | `"brightWhite"`       | Pure white |

### Background Colors

Add 10 to the foreground color code to get the background equivalent:
- **Standard backgrounds**: `40-47` (e.g., `\033[41m` for red background)
- **Bright backgrounds**: `100-107` (e.g., `\033[101m` for bright red background)

### Reset Code
**Always remember**: Use `\033[0m` to reset colors back to default!

---

## Extended Color Modes

### 256-Color Mode (8-bit color)
```
# Foreground: \033[38;5;Nm where N = 0-255
# Background: \033[48;5;Nm where N = 0-255

# Examples:
\033[38;5;196m  # Bright red (color 196)
\033[48;5;21m   # Blue background (color 21)
```

### True Color Mode (24-bit RGB)
```
# Foreground: \033[38;2;R;G;Bm 
# Background: \033[48;2;R;G;Bm
# Where R, G, B are values from 0-255

# Examples:
\033[38;2;255;100;50m   # Orange text
\033[48;2;25;25;112m    # Midnight blue background
```

---

## Plain-Language Clarity Notes

### For Beginners:
1. **Start simple**: Use the 8 basic colors first
2. **Think in pairs**: Most applications need both text color and background color
3. **Always reset**: End your color sequences with `\033[0m`
4. **Test first**: Colors look different on different screens and themes

### Common Confusions Clarified:
- **"ANSI colors" = "ECMA-48 colors"** - Same thing, different names
- **"Bright black" = "Gray"** - Historical naming that stuck
- **"Purple" vs "Magenta"** - Windows Terminal uses "purple" in settings, but it's technically magenta
- **Color numbers vs names** - Use names in settings.json, numbers in escape sequences

### Accessibility Notes:
- **High contrast**: Consider users with visual impairments
- **Color blindness**: Don't rely solely on color to convey information
- **Cultural differences**: Color meanings vary across cultures

---

## Practical Examples

### Basic Shell Usage
```bash
# Bash/Zsh examples
echo -e "\033[31mThis text is red\033[0m"
echo -e "\033[42mThis has a green background\033[0m"
echo -e "\033[1;33;44mBold yellow text on blue background\033[0m"
```

### PowerShell Examples
```powershell
# Using Write-Host with color names (PowerShell built-in)
Write-Host "Success!" -ForegroundColor Green
Write-Host "Warning!" -ForegroundColor Yellow -BackgroundColor Black

# Using ANSI escape sequences (PowerShell 7+)
Write-Host "`e[32mThis is green text`e[0m"
Write-Host "`e[91mThis is bright red text`e[0m"
```

### Windows Terminal Settings
```json
{
    "name": "My Custom Scheme",
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

---

## Code References and Links

### Windows Terminal Specific:
- [Windows Terminal Documentation](https://docs.microsoft.com/en-us/windows/terminal/)
- [Color Scheme Reference](https://docs.microsoft.com/en-us/windows/terminal/customize-settings/color-schemes)
- [Windows Terminal Source Code](https://github.com/microsoft/terminal)

### Standards and Specifications:
- [ECMA-48 Standard Document](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- [ISO/IEC 6429](https://www.iso.org/standard/12782.html)
- [ANSI Escape Code Wikipedia](https://en.wikipedia.org/wiki/ANSI_escape_code)

### Tools and Resources:
- [Color Scheme Gallery](https://windowsterminalthemes.dev/)
- [Terminal Color Picker](https://terminal.sexy/)
- [ANSI Color Test Scripts](https://github.com/robertknight/konsole/tree/master/tests)

### Code Examples:
```bash
# Test all 16 colors script
for i in {30..37}; do
    echo -e "\033[${i}m Color $i \033[0m"
done
for i in {90..97}; do  
    echo -e "\033[${i}m Bright Color $i \033[0m"
done
```

---

## Summary

Windows Terminal implements the **ECMA-48 standard** for color control, providing:

✅ **Full compatibility** with ANSI escape sequences  
✅ **User-friendly color names** in configuration files  
✅ **Support for all color modes**: 8-color, 16-color, 256-color, and true color  
✅ **Beginner-friendly** configuration through GUI settings  
✅ **Advanced customization** through JSON configuration  

### Key Takeaways for Issue #2641:
1. **Terminology clarity**: ANSI colors = ECMA-48 colors
2. **Simple mapping**: Use the tables above for quick reference
3. **Plain language**: "Bright black" = gray, "purple" = magenta
4. **Practical examples**: Copy-paste ready code snippets
5. **Comprehensive resources**: Links to official documentation and tools

This document should help both newcomers understand color basics and experienced users implement advanced color schemes effectively.

---

*This documentation addresses issue #2641 by providing clear explanations of confusing color terminology, practical mapping tables, and beginner-friendly guidance for Windows Terminal color implementation.*
