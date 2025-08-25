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

| Color Index | ANSI Code | Color Name | Windows Terminal Name | Plain Language |
|-------------|-----------|------------|----------------------|----------------|
| 0 | 30 / 40 | Black | `black` | Black |
| 1 | 31 / 41 | Red | `red` | Red |
| 2 | 32 / 42 | Green | `green` | Green |
| 3 | 33 / 43 | Yellow | `yellow` | Yellow |
| 4 | 34 / 44 | Blue | `blue` | Blue |
| 5 | 35 / 45 | Magenta | `purple` | Purple/Magenta |
| 6 | 36 / 46 | Cyan | `cyan` | Cyan/Light Blue |
| 7 | 37 / 47 | White | `white` | Light Gray |

**Note**: ANSI codes 30-37 are for foreground (text color), 40-47 are for background.

### Bright Colors (4-bit color extension)

| Color Index | ANSI Code | Color Name | Windows Terminal Name | Plain Language |
|-------------|-----------|------------|----------------------|----------------|
| 8 | 90 / 100 | Bright Black | `brightBlack` | Dark Gray |
| 9 | 91 / 101 | Bright Red | `brightRed` | Bright Red |
| 10 | 92 / 102 | Bright Green | `brightGreen` | Bright Green |
| 11 | 93 / 103 | Bright Yellow | `brightYellow` | Bright Yellow |
| 12 | 94 / 104 | Bright Blue | `brightBlue` | Bright Blue |
| 13 | 95 / 105 | Bright Magenta | `brightPurple` | Bright Purple |
| 14 | 96 / 106 | Bright Cyan | `brightCyan` | Bright Cyan |
| 15 | 97 / 107 | Bright White | `brightWhite` | White |

**Note**: ANSI codes 90-97 are for bright foreground, 100-107 are for bright background.

---

## Confusing Terms Explained

### "Bright Black" vs "Black"

- **Black (Index 0)**: True black `#000000`
- **Bright Black (Index 8)**: Actually **dark gray** `#808080`

### "Purple" vs "Magenta"

- Windows Terminal uses `purple` in configuration files
- The ANSI standard calls it "magenta"
- **They refer to the same color** (Index 5)

### "White" vs "Bright White"

- **White (Index 7)**: Actually **light gray**
- **Bright White (Index 15)**: True white `#FFFFFF`

---

## Practical Examples

### Basic ANSI Escape Sequences

```bash
# Red text
echo -e "\033[31mRed text\033[0m"

# Blue background
echo -e "\033[44mBlue background\033[0m"

# Bright green text
echo -e "\033[92mBright green text\033[0m"

# Reset to default
echo -e "\033[0mBack to normal"
```

### Windows Terminal JSON Configuration

```json
{
    "name": "Custom Color Scheme",
    "foreground": "#CCCCCC",
    "background": "#0C0C0C",
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
    "brightWhite": "#F2F2F2"
}
```

---

## Extended Color Support

### 256-Color Mode (8-bit color)

- **Colors 0-15**: Same as the 16-color palette above
- **Colors 16-231**: 216 colors in a 6×6×6 RGB cube
- **Colors 232-255**: 24 grayscale colors

**Usage**: `\033[38;5;COLORm` for foreground, `\033[48;5;COLORm` for background

### True Color Mode (24-bit color)

**Usage**: `\033[38;2;R;G;Bm` for foreground, `\033[48;2;R;G;Bm` for background

**Example**:
```bash
# Orange text (RGB: 255, 165, 0)
echo -e "\033[38;2;255;165;0mOrange text\033[0m"
```

---

## Additional Resources

### Standards and Specifications:

- **ECMA-48 Standard Document**: https://ecma-international.org/publications-and-standards/standards/ecma-48/
- **ISO/IEC 6429**: https://www.iso.org/standard/12782.html
- **ANSI Escape Code Wikipedia**: https://en.wikipedia.org/wiki/ANSI_escape_code

### Tools and Resources:

- **Color Scheme Gallery**: https://windowsterminalthemes.dev/
- **Terminal Color Picker**: https://terminal.sexy/
- **ANSI Color Test Scripts**: https://github.com/robertknight/konsole/tree/master/tests

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

### Key Points for Issue #2641:

1. **Terminology clarity**: ANSI colors = ECMA-48 colors
2. **Simple mapping**: Use the tables above for quick reference
3. **Plain language**: "Bright black" = gray, "purple" = magenta
4. **Practical examples**: Copy-paste ready code snippets
5. **Comprehensive resources**: Links to official documentation and tools

This document should help both newcomers understand color basics and experienced users implement advanced color schemes effectively.

---

*This documentation addresses issue #2641 by providing clear explanations of confusing color terminology, practical mapping tables, and beginner-friendly guidance for Windows Terminal color implementation.*
