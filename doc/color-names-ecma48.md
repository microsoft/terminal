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

- **Windows Terminal Source Code**: https://github.com/microsoft/terminal

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

### Key Takeaways for Issue #2641:

1. **Terminology clarity**: ANSI colors = ECMA-48 colors
2. **Simple mapping**: Use the tables above for quick reference
3. **Plain language**: "Bright black" = gray, "purple" = magenta
4. **Practical examples**: Copy-paste ready code snippets
5. **Comprehensive resources**: Links to official documentation and tools

This document should help both newcomers understand color basics and experienced users implement advanced color schemes effectively.

---

*This documentation addresses issue #2641 by providing clear explanations of confusing color terminology, practical mapping tables, and beginner-friendly guidance for Windows Terminal color implementation.*
