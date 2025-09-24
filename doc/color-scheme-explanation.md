# Windows Terminal Color Names

Windows Terminal uses a 16-color naming scheme for color schemes. If you've been confused by names like `brightBlack` or `brightWhite`, this guide explains what each name actually represents.

## Color Names Reference

| Index | Terminal Name | Visual | RGB (Campbell) | Description |
|-------|---------------|--------|----------------|-------------|
| 0 | `black` | ![#0C0C0C](https://placehold.co/20x20/0c0c0c/0c0c0c.png) | `#0C0C0C` | Darkest shade |
| 1 | `red` | ![#C50F1F](https://placehold.co/20x20/c50f1f/c50f1f.png) | `#C50F1F` | Standard red |
| 2 | `green` | ![#13A10E](https://placehold.co/20x20/13A10E/13A10E.png) | `#13A10E` | Standard green |
| 3 | `yellow` | ![#C19C00](https://placehold.co/20x20/C19C00/C19C00.png) | `#C19C00` | Standard yellow |
| 4 | `blue` | ![#0037DA](https://placehold.co/20x20/0037DA/0037DA.png) | `#0037DA` | Standard blue |
| 5 | `purple` | ![#881798](https://placehold.co/20x20/881798/881798.png) | `#881798` | Standard purple* |
| 6 | `cyan` | ![#3A96DD](https://placehold.co/20x20/3A96DD/3A96DD.png) | `#3A96DD` | Standard cyan |
| 7 | `white` | ![#CCCCCC](https://placehold.co/20x20/CCCCCC/CCCCCC.png) | `#CCCCCC` | Light gray |
| 8 | `brightBlack` | ![#767676](https://placehold.co/20x20/767676/767676.png) | `#767676` | **Dark gray** |
| 9 | `brightRed` | ![#E74856](https://placehold.co/20x20/E74856/E74856.png) | `#E74856` | Bright red |
| 10 | `brightGreen` | ![#16C60C](https://placehold.co/20x20/16C60C/16C60C.png) | `#16C60C` | Bright green |
| 11 | `brightYellow` | ![#F9F1A5](https://placehold.co/20x20/F9F1A5/F9F1A5.png) | `#F9F1A5` | Bright yellow |
| 12 | `brightBlue` | ![#3B78FF](https://placehold.co/20x20/3B78FF/3B78FF.png) | `#3B78FF` | Bright blue |
| 13 | `brightPurple` | ![#B4009E](https://placehold.co/20x20/B4009E/B4009E.png) | `#B4009E` | Bright purple* |
| 14 | `brightCyan` | ![#61D6D6](https://placehold.co/20x20/61D6D6/61D6D6.png) | `#61D6D6` | Bright cyan |
| 15 | `brightWhite` | ![#F2F2F2](https://placehold.co/20x20/F2F2F2/F2F2F2.png) | `#F2F2F2` | **Pure white** |

*Note: `purple` should technically be `magenta` per ANSI standards. This is a known inconsistency. The two are functionally identical.

## Usage Examples

### Basic Color Scheme
```json
{
  "name": "MyScheme",
  "background": "#0C0C0C",
  "foreground": "#CCCCCC",
  "black": "#0C0C0C",
  "brightBlack": "#767676",
  "white": "#CCCCCC", 
  "brightWhite": "#F2F2F2"
}
```

### Common Patterns
```json
{
  "name": "FullCampbell",
  "background": "#0C0C0C",        // Background (black)
  "foreground": "#CCCCCC",        // Default text (light gray)
  "cursorColor": "#FFFFFF",       // Cursor color
  "selectionBackground": "#767676", // Selection highlight (dark gray)

  // Standard colors (0–7)
  "black": "#0C0C0C",
  "red": "#C50F1F",
  "green": "#13A10E",
  "yellow": "#C19C00",
  "blue": "#0037DA",
  "purple": "#881798",
  "cyan": "#3A96DD",
  "white": "#CCCCCC",

  // Bright colors (8–15)
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
### How to Use This Scheme

1. Open **Windows Terminal Settings** → `settings.json`.
2. Find the `"schemes"` section and paste the JSON block inside it.
3. Then, tell Windows Terminal to use it by setting `"colorScheme"` in your profile:

```jsonc
"profiles": {
  "defaults": {
    "colorScheme": "FullCampbell"
  }
}
```

## Common Questions

### Why is `brightBlack` not black?
`brightBlack` represents **dark gray**, not black. In terminal color systems, `black` is the darkest possible color, while `brightBlack` is a lighter shade used for dark gray elements like comments or dimmed text.

### `white` vs `brightWhite` explained
- `white` = Light gray (`#CCCCCC` in Campbell scheme)
- `brightWhite` = Pure white (`#F2F2F2` in Campbell scheme)

This distinction allows color schemes to have both a readable light gray and a pure white color.

### Why `purple` instead of `magenta`?
This is a known inconsistency. The ANSI standard uses `magenta`, but Windows Terminal currently uses `purple`. This may be corrected in future versions.

## Comparison with .NET ConsoleColor

| Windows Terminal | .NET ConsoleColor | Notes |
|------------------|-------------------|--------|
| `black` | `Black` | Same concept |
| `brightBlack` | `DarkGray` | More descriptive .NET name |
| `white` | `Gray` | .NET uses Gray for light gray |
| `brightWhite` | `White` | .NET uses White for pure white |
| `red` | `DarkRed` | .NET distinguishes dark vs bright |
| `brightRed` | `Red` | .NET's "Red" is actually bright red |
| `blue` | `DarkBlue` | Same pattern as red |
| `brightBlue` | `Blue` | Same pattern as red |

### Migration from .NET Names
If you're familiar with .NET's `ConsoleColor` enum, here's a quick conversion:

```
.NET DarkGray    → brightBlack
.NET Gray        → white  
.NET White       → brightWhite
.NET DarkRed     → red
.NET Red         → brightRed
```

## Background: Why These Names?

Windows Terminal's color names follow the ECMA-48 standard (later ANSI X3.64), which defined terminal colors in 1976. The standard specified 8 base colors plus an "intensity" attribute that could make them brighter.

Early terminals could only display 8 colors at once, but the intensity attribute effectively created 16 colors:
- Standard colors (0-7): The original 8 colors
- Bright colors (8-15): The same colors with intensity applied

This is why we have pairs like `red`/`brightRed` and why `brightBlack` means "black with intensity" (which appears as dark gray).

### Why Not Use .NET Names?

While .NET's `ConsoleColor` names like `DarkGray` might seem clearer than `brightBlack`, changing Windows Terminal's names would break existing color schemes and configurations. The ECMA-48 naming is also consistent with other terminal emulators and shell environments.

## See Also

- [ANSI escape code standard](https://en.wikipedia.org/wiki/ANSI_escape_code#Colors)
- [ConsoleColor enum](https://learn.microsoft.com/en-us/dotnet/api/system.consolecolor?view=netframework-4.8)
