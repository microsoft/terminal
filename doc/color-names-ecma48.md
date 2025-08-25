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

## Basic Color Codes (8-Color Mode)

The most basic color support includes 8 colors:

| Color Name | Foreground Code | Background Code | Example |
|------------|----------------|----------------|---------|
| Black      | `\033[30m`     | `\033[40m`     | `\033[30mBlack text\033[0m` |
| Red        | `\033[31m`     | `\033[41m`     | `\033[31mRed text\033[0m` |
| Green      | `\033[32m`     | `\033[42m`     | `\033[32mGreen text\033[0m` |
| Yellow     | `\033[33m`     | `\033[43m`     | `\033[33mYellow text\033[0m` |
| Blue       | `\033[34m`     | `\033[44m`     | `\033[34mBlue text\033[0m` |
| Magenta    | `\033[35m`     | `\033[45m`     | `\033[35mMagenta text\033[0m` |
| Cyan       | `\033[36m`     | `\033[46m`     | `\033[36mCyan text\033[0m` |
| White      | `\033[37m`     | `\033[47m`     | `\033[37mWhite text\033[0m` |

### Reset Code
- `\033[0m` - Reset to default colors

---

## Extended Colors (16-Color Mode)

Most modern terminals support "bright" variations of the 8 basic colors:

| Color Name    | Foreground Code | Background Code | Note |
|---------------|----------------|----------------|---------|
| Bright Black  | `\033[90m`     | `\033[100m`    | Actually gray |
| Bright Red    | `\033[91m`     | `\033[101m`    | Lighter red |
| Bright Green  | `\033[92m`     | `\033[102m`    | Lighter green |
| Bright Yellow | `\033[93m`     | `\033[103m`    | Lighter yellow |
| Bright Blue   | `\033[94m`     | `\033[104m`    | Lighter blue |
| Bright Magenta| `\033[95m`     | `\033[105m`    | Lighter magenta |
| Bright Cyan   | `\033[96m`     | `\033[106m`    | Lighter cyan |
| Bright White  | `\033[97m`     | `\033[107m`    | Brighter white |

### Plain Language Notes:
- "Bright black" is confusing - it's actually **gray**
- "Magenta" is what most people call **purple**
- "Cyan" is **light blue** or **aqua**

---

## 256-Color Mode

For more color options, use the 256-color palette:

### Syntax:
- Foreground: `\033[38;5;<n>m`
- Background: `\033[48;5;<n>m`

<!-- <n> = color index (0-255) -->

### Color Ranges:
- **0-15**: Standard 16 colors (same as above)
- **16-231**: 216 colors in a 6×6×6 RGB cube
- **232-255**: 24 grayscale colors

### Examples:
```bash
# Red foreground (color 196)
echo -e "\033[38;5;196mBright red text\033[0m"

# Blue background (color 21)
echo -e "\033[48;5;21mText on blue background\033[0m"

# Both foreground and background
echo -e "\033[38;5;226;48;5;18mYellow on dark blue\033[0m"
```

---

## True Color (24-bit RGB)

For maximum color precision, use RGB values directly:

### Syntax:
- Foreground: `\033[38;2;r;g;b m`
- Background: `\033[48;2;r;g;b m`

Where r, g, b are values from 0-255.

### Examples:
```bash
# Pure red foreground (RGB 255,0,0)
echo -e "\033[38;2;255;0;0mPure red text\033[0m"

# Orange background (RGB 255,165,0)
echo -e "\033[48;2;255;165;0mText on orange background\033[0m"

# Custom purple (RGB 128,0,128)
echo -e "\033[38;2;128;0;128mCustom purple text\033[0m"
```

---

## Windows Terminal Color Names

Windows Terminal provides user-friendly names in configuration files:

### Settings.json Color Names:
```json
{
  "profiles": {
    "defaults": {
      "colorScheme": "Campbell"
    }
  },
  "schemes": [
    {
      "name": "My Custom Theme",
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
  ]
}
```

### Color Name Mapping:
| JSON Name | ANSI Color | Note |
|-----------|------------|------|
| black | Black | Standard black |
| red | Red | Standard red |
| green | Green | Standard green |
| yellow | Yellow | Standard yellow |
| blue | Blue | Standard blue |
| purple | Magenta | **Note: Called "purple" in JSON, "magenta" in ANSI** |
| cyan | Cyan | Standard cyan |
| white | White | Standard white |
| brightBlack | Bright Black | **Actually gray** |
| brightRed | Bright Red | Lighter red |
| brightGreen | Bright Green | Lighter green |
| brightYellow | Bright Yellow | Lighter yellow |
| brightBlue | Bright Blue | Lighter blue |
| brightPurple | Bright Magenta | Lighter purple/magenta |
| brightCyan | Bright Cyan | Lighter cyan |
| brightWhite | Bright White | Brighter white |

---

## Common Escape Sequence Patterns

### Text Formatting:
- `\033[1m` - Bold
- `\033[2m` - Dim
- `\033[3m` - Italic
- `\033[4m` - Underline
- `\033[7m` - Reverse (swap foreground/background)
- `\033[9m` - Strikethrough

### Cursor Control:
- `\033[H` - Move cursor to home position
- `\033[2J` - Clear entire screen
- `\033[K` - Clear to end of line

### Combining Codes:
You can combine multiple codes with semicolons:
```bash
# Bold red text on white background
echo -e "\033[1;31;47mBold red on white\033[0m"

# Underlined blue text
echo -e "\033[4;34mUnderlined blue text\033[0m"
```

---

## Troubleshooting Common Issues

### Issue: Colors Don't Display
**Cause**: Terminal doesn't support color
**Solution**: Check terminal settings or use a modern terminal emulator

### Issue: Wrong Colors Appear
**Cause**: Terminal color scheme interference
**Solution**: Use specific RGB values instead of named colors

### Issue: Color Codes Show as Text
**Cause**: Terminal doesn't interpret escape sequences
**Solution**: 
- In bash: Use `echo -e` instead of `echo`
- In PowerShell: Enable virtual terminal processing
- In cmd: Use Windows 10 version 1909 or later

### Issue: Colors Persist After Reset
**Cause**: Missing reset code
**Solution**: Always end colored text with `\033[0m`

---

## Additional Resources

### Official Documentation:
- **Windows Terminal Documentation**: https://docs.microsoft.com/en-us/windows/terminal/
- **ECMA-48 Standard**: https://www.ecma-international.org/publications/standards/Ecma-048.htm
- **ISO/IEC 6429**: https://www.iso.org/standard/12782.html

### Tools and Utilities:
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
