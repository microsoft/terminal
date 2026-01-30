# VT Sequences in Windows Terminal

> A comprehensive guide to Virtual Terminal (VT) sequences and how Windows Terminal processes them
>
> Generated: 2026-01-26

## Table of Contents

1. [What are VT Sequences?](#what-are-vt-sequences)
2. [Why VT Sequences Matter](#why-vt-sequences-matter)
3. [VT Sequence Types](#vt-sequence-types)
4. [How Windows Terminal Processes VT Sequences](#how-windows-terminal-processes-vt-sequences)
5. [Common VT Sequence Examples](#common-vt-sequence-examples)
6. [The State Machine](#the-state-machine)
7. [Code Flow Through Windows Terminal](#code-flow-through-windows-terminal)
8. [Practical Examples](#practical-examples)
9. [Writing Programs That Use VT Sequences](#writing-programs-that-use-vt-sequences)

---

## What are VT Sequences?

**VT sequences** (Virtual Terminal sequences), also known as **ANSI escape sequences** or **control sequences**, are special character sequences that control terminal behavior. They originated with the **DEC VT100** terminal in 1978 and have become the de facto standard for terminal control.

### Basic Structure

VT sequences always start with special characters:
- **ESC** (Escape character, ASCII 27, `\x1b` or `\033`)
- Followed by additional characters that define the action

```
ESC [  3  1  m     ← Set text color to red
 │   │  └─┴──┴── Parameters
 │   └── Control Sequence Introducer (CSI)
 └── Escape character
```

### Why They Exist

Before graphical interfaces, terminals were hardware devices (like the VT100) connected to computers. Applications needed a way to:
- Move the cursor around the screen
- Change text colors
- Clear the screen
- Position text at specific coordinates
- Control scrolling
- And much more

VT sequences provided a **standardized protocol** for these operations. Today, terminal emulators like Windows Terminal implement this same protocol for compatibility with thousands of console applications.

---

## Why VT Sequences Matter

### In Windows Terminal Context

When you run a program like:
- `vim` - Text editor with colors and cursor positioning
- `htop` - System monitor with dynamic updates
- `git diff` - Shows colored diffs
- `pytest` - Displays test results with colors
- Any TUI (Text User Interface) application

These programs **don't directly draw to your screen**. Instead, they:

1. Generate VT sequences as text output
2. Send them to the terminal (via stdout/ConPTY)
3. Windows Terminal **parses** these sequences
4. Windows Terminal **executes** the commands (move cursor, change color, etc.)
5. Windows Terminal **renders** the final visual result

```
┌─────────────────────────────────────────────────────────┐
│  Application (e.g., vim, htop, git)                     │
│  Generates VT sequences:                                │
│  "Hello" + ESC[31m + "World" + ESC[0m                  │
└────────────────────┬────────────────────────────────────┘
                     │ (writes to stdout)
                     ↓
┌─────────────────────────────────────────────────────────┐
│  Windows ConPTY (Pseudoconsole)                         │
│  Forwards VT sequences to terminal                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  ConptyConnection::_OutputThread()                      │
│  Reads output, fires TerminalOutput event               │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  Terminal::Write()                                      │
│  Processes character stream                             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  StateMachine::ProcessString()                          │
│  PARSES VT sequences character by character             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  AdaptDispatch::Execute()                               │
│  EXECUTES the parsed commands                           │
│  • Print() - Regular text                               │
│  • SetGraphicsRendition() - Colors                      │
│  • CursorPosition() - Move cursor                       │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  TextBuffer updated                                     │
│  Characters, colors, cursor position stored             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────┐
│  Renderer::TriggerRedraw()                              │
│  GPU renders final image to screen                      │
└─────────────────────────────────────────────────────────┘
```

---

## VT Sequence Types

Windows Terminal supports several types of VT sequences, each with a different introducer and purpose.

### 1. CSI Sequences (Control Sequence Introducer)

**Format:** `ESC [ <parameters> <command>`

**Most Common Type** - Used for cursor movement, colors, erasing, etc.

**Examples:**
```
ESC[H          ← Move cursor to home (1,1)
ESC[2J         ← Clear entire screen
ESC[31m        ← Set foreground color to red
ESC[10;5H      ← Move cursor to row 10, column 5
ESC[1A         ← Move cursor up 1 line
```

**In Windows Terminal Code:**
Located in `src/terminal/parser/OutputStateMachineEngine.hpp`:

```cpp
enum CsiActionCodes : uint64_t
{
    ICH_InsertCharacter = VTID("@"),           // ESC[@
    CUU_CursorUp = VTID("A"),                  // ESC[A
    CUD_CursorDown = VTID("B"),                // ESC[B
    CUF_CursorForward = VTID("C"),             // ESC[C
    CUB_CursorBackward = VTID("D"),            // ESC[D
    CNL_CursorNextLine = VTID("E"),            // ESC[E
    CPL_CursorPrevLine = VTID("F"),            // ESC[F
    CHA_CursorHorizontalAbsolute = VTID("G"),  // ESC[G
    CUP_CursorPosition = VTID("H"),            // ESC[H
    ED_EraseDisplay = VTID("J"),               // ESC[J
    EL_EraseLine = VTID("K"),                  // ESC[K
    SGR_SetGraphicsRendition = VTID("m"),      // ESC[m
    // ... many more
};
```

### 2. OSC Sequences (Operating System Command)

**Format:** `ESC ] <number> ; <text> BEL` or `ESC ] <number> ; <text> ESC \`

**Purpose:** Set window title, colors, clipboard, hyperlinks, etc.

**Examples:**
```
ESC]0;My Window Title\x07     ← Set window title
ESC]8;;http://example.com\x07 ← Start hyperlink
ESC]8;;\x07                    ← End hyperlink
ESC]4;1;rgb:ff/00/00\x07       ← Set color palette entry 1 to red
ESC]52;c;base64data\x07        ← Set clipboard
```

**In Windows Terminal Code:**
```cpp
enum OscActionCodes : unsigned int
{
    SetIconAndWindowTitle = 0,
    SetWindowIcon = 1,
    SetWindowTitle = 2,
    SetColor = 4,
    Hyperlink = 8,
    ConEmuAction = 9,
    SetForegroundColor = 10,
    SetBackgroundColor = 11,
    SetCursorColor = 12,
    SetClipboard = 52,
    ResetColor = 104,
    ITerm2Action = 1337,
    WTAction = 9001,  // Windows Terminal specific
};
```

### 3. ESC Sequences (Simple Escapes)

**Format:** `ESC <command>`

**Purpose:** Simple single-character commands

**Examples:**
```
ESC 7     ← Save cursor position (DECSC)
ESC 8     ← Restore cursor position (DECRC)
ESC D     ← Index (move cursor down, scroll if at bottom)
ESC M     ← Reverse Index (move cursor up, scroll if at top)
ESC c     ← Reset to Initial State (RIS)
ESC =     ← Application keypad mode
ESC >     ← Numeric keypad mode
```

**In Windows Terminal Code:**
```cpp
enum EscActionCodes : uint64_t
{
    DECBI_BackIndex = VTID("6"),
    DECSC_CursorSave = VTID("7"),
    DECRC_CursorRestore = VTID("8"),
    DECFI_ForwardIndex = VTID("9"),
    DECKPAM_KeypadApplicationMode = VTID("="),
    DECKPNM_KeypadNumericMode = VTID(">"),
    IND_Index = VTID("D"),
    NEL_NextLine = VTID("E"),
    HTS_HorizontalTabSet = VTID("H"),
    RI_ReverseLineFeed = VTID("M"),
    RIS_ResetToInitialState = VTID("c"),
};
```

### 4. DCS Sequences (Device Control String)

**Format:** `ESC P <parameters> <data> ESC \`

**Purpose:** Complex operations like SIXEL graphics, macros, downloadable character sets

**Examples:**
```
ESC P q <sixel data> ESC \    ← SIXEL image
ESC P $ q <setting> ESC \      ← Request setting
```

**In Windows Terminal Code:**
```cpp
enum DcsActionCodes : uint64_t
{
    SIXEL_DefineImage = VTID("q"),
    DECDLD_DownloadDRCS = VTID("{"),
    DECDMAC_DefineMacro = VTID("!z"),
    DECRSTS_RestoreTerminalState = VTID("$p"),
    DECRQSS_RequestSetting = VTID("$q"),
};
```

### 5. C0 and C1 Control Characters

**Single control characters** (not escape sequences, but related):

```
\x07  BEL  - Bell (beep)
\x08  BS   - Backspace
\x09  HT   - Horizontal Tab
\x0A  LF   - Line Feed (newline)
\x0D  CR   - Carriage Return
\x1B  ESC  - Escape (starts sequences)
```

---

## How Windows Terminal Processes VT Sequences

### The Parsing Pipeline

```
Character Stream Input:
  "Hello\x1b[31mWorld\x1b[0m"
         └────┴── ESC[31m (red foreground)
                      └────┴── ESC[0m (reset)

           ↓

┌────────────────────────────────────────────────────┐
│  StateMachine (state machine parser)               │
│  File: src/terminal/parser/stateMachine.cpp        │
│                                                    │
│  Parses character by character:                    │
│  • Regular characters → Print action               │
│  • ESC → Enter escape state                        │
│  • [ after ESC → Enter CSI state                   │
│  • Collect parameters (31)                         │
│  • Final character (m) → Dispatch SGR              │
└────────────────────┬───────────────────────────────┘
                     │
                     ↓
┌────────────────────────────────────────────────────┐
│  OutputStateMachineEngine                          │
│  File: src/terminal/parser/OutputStateMachineEngine│
│                                                    │
│  Routes parsed sequences to handlers:              │
│  • ActionPrint() → Regular text                    │
│  • ActionCsiDispatch() → CSI sequences             │
│  • ActionOscDispatch() → OSC sequences             │
│  • ActionEscDispatch() → Simple escapes            │
└────────────────────┬───────────────────────────────┘
                     │
                     ↓
┌────────────────────────────────────────────────────┐
│  AdaptDispatch (command executor)                  │
│  File: src/terminal/adapter/adaptDispatch.cpp     │
│                                                    │
│  Executes the actual terminal operations:          │
│  • Print("Hello") → Write to buffer                │
│  • SetGraphicsRendition(31) → Set red color        │
│  • Print("World") → Write with red color           │
│  • SetGraphicsRendition(0) → Reset colors          │
└────────────────────┬───────────────────────────────┘
                     │
                     ↓
┌────────────────────────────────────────────────────┐
│  Terminal / TextBuffer                             │
│  File: src/cascadia/TerminalCore/Terminal.cpp     │
│                                                    │
│  Updates internal state:                           │
│  • TextBuffer stores characters + attributes       │
│  • Cursor position updated                         │
│  • Colors applied to cells                         │
└────────────────────┬───────────────────────────────┘
                     │
                     ↓
┌────────────────────────────────────────────────────┐
│  Renderer                                          │
│  File: src/renderer/base/renderer.cpp             │
│                                                    │
│  Visual output:                                    │
│  • Reads TextBuffer state                          │
│  • Renders: "Hello" (normal) "World" (red)         │
└────────────────────────────────────────────────────┘
```

### State Machine States

The parser (`StateMachine`) maintains a state machine with these states:

**File:** `src/terminal/parser/stateMachine.hpp` (line 166)

```cpp
enum class VTStates
{
    Ground,              // Normal text
    Escape,              // After ESC
    EscapeIntermediate,  // After ESC and intermediate char
    CsiEntry,            // After ESC[
    CsiIntermediate,     // During CSI with intermediate
    CsiIgnore,           // CSI we don't understand
    CsiParam,            // Collecting CSI parameters
    CsiSubParam,         // Collecting CSI sub-parameters
    OscParam,            // Collecting OSC parameter number
    OscString,           // Collecting OSC string data
    OscTermination,      // OSC terminator (BEL or ESC\)
    Ss3Entry,            // After ESC O (SS3)
    Ss3Param,            // Collecting SS3 parameters
    Vt52Param,           // VT52 mode parameters
    DcsEntry,            // After ESC P (DCS)
    DcsIgnore,           // DCS we don't understand
    DcsIntermediate,     // DCS intermediate
    DcsParam,            // DCS parameters
    DcsPassThrough,      // DCS data passthrough
    SosPmApcString       // Other string types
};
```

### Example: Parsing `ESC[31m`

```
Input: "\x1b[31m"

Character │ Current State    │ Action                     │ Next State
──────────┼──────────────────┼────────────────────────────┼─────────────────
\x1b      │ Ground           │ Enter escape mode          │ Escape
[         │ Escape           │ Enter CSI mode             │ CsiEntry
3         │ CsiEntry         │ Accumulate param (3)       │ CsiParam
1         │ CsiParam         │ Accumulate param (31)      │ CsiParam
m         │ CsiParam         │ Dispatch SGR(31), return   │ Ground
```

**Dispatched Command:** `SetGraphicsRendition(31)`
- Parameter 31 = Red foreground color
- AdaptDispatch updates the current text attribute
- Subsequent text will be rendered in red

---

## Common VT Sequence Examples

### Cursor Movement

```bash
# Move cursor to row 10, column 5
echo -e "\033[10;5H"

# Move cursor up 3 lines
echo -e "\033[3A"

# Move cursor down 1 line
echo -e "\033[B"

# Move cursor forward 5 columns
echo -e "\033[5C"

# Move cursor backward 2 columns
echo -e "\033[2D"

# Move cursor to beginning of line
echo -e "\033[G"

# Save cursor position
echo -e "\0337"

# Restore cursor position
echo -e "\0338"
```

**Windows Terminal Handling:**
- Parsed by StateMachine
- Dispatched to `AdaptDispatch::CursorPosition()`, `CursorUp()`, etc.
- Updates cursor position in Terminal
- Next render shows cursor at new position

### Colors and Text Attributes

```bash
# Set foreground color to red (bright)
echo -e "\033[31m"

# Set foreground color to bright green
echo -e "\033[92m"

# Set background color to blue
echo -e "\033[44m"

# Set bold
echo -e "\033[1m"

# Set underline
echo -e "\033[4m"

# Reset all attributes
echo -e "\033[0m"

# 256-color foreground
echo -e "\033[38;5;208m"  # Orange

# RGB (true color) foreground
echo -e "\033[38;2;255;128;0m"  # Orange

# RGB background
echo -e "\033[48;2;0;0;128m"  # Dark blue
```

**SGR (Select Graphic Rendition) Parameters:**

| Code | Effect |
|------|--------|
| 0 | Reset all attributes |
| 1 | Bold |
| 2 | Dim |
| 3 | Italic |
| 4 | Underline |
| 5 | Slow blink |
| 7 | Reverse video (swap fg/bg) |
| 9 | Strikethrough |
| 22 | Normal intensity (not bold/dim) |
| 24 | Not underlined |
| 27 | Not reversed |
| 30-37 | Foreground color (black to white) |
| 38;5;N | 256-color foreground (N = 0-255) |
| 38;2;R;G;B | RGB foreground |
| 40-47 | Background color (black to white) |
| 48;5;N | 256-color background |
| 48;2;R;G;B | RGB background |
| 90-97 | Bright foreground colors |
| 100-107 | Bright background colors |

**Windows Terminal Handling:**
- `SetGraphicsRendition()` in AdaptDispatch
- Updates current TextAttribute
- Applies to subsequent characters written to buffer

### Screen Manipulation

```bash
# Clear entire screen
echo -e "\033[2J"

# Clear from cursor to end of screen
echo -e "\033[0J"

# Clear from cursor to beginning of screen
echo -e "\033[1J"

# Clear entire line
echo -e "\033[2K"

# Clear from cursor to end of line
echo -e "\033[0K"

# Clear from cursor to beginning of line
echo -e "\033[1K"

# Scroll up 5 lines
echo -e "\033[5S"

# Scroll down 3 lines
echo -e "\033[3T"
```

**Windows Terminal Handling:**
- `EraseInDisplay()`, `EraseInLine()`, `ScrollUp()`, `ScrollDown()` in AdaptDispatch
- Modifies TextBuffer directly
- Clears/scrolls buffer regions
- Triggers redraw of affected areas

### Window Title

```bash
# Set window title
echo -e "\033]0;My Terminal Window\007"

# Alternative with ESC\ terminator
echo -e "\033]0;My Terminal Window\033\\"

# Set icon name and window title
echo -e "\033]0;Icon:Window Title\007"
```

**Windows Terminal Handling:**
- OSC sequence (Operating System Command)
- `SetWindowTitle()` in AdaptDispatch
- Fires event that propagates up to TerminalApp
- Updates window title in UI

### Hyperlinks

```bash
# Create clickable hyperlink
echo -e "\033]8;;https://github.com\033\\GitHub\033]8;;\033\\"
#        \_____/\_____________________/     \_____/\______/
#           │           │                      │       │
#     Start link   URL to link              Link text  End link

# With ID parameter (multiple links to same URL share ID)
echo -e "\033]8;id=123;https://example.com\033\\Click Here\033]8;;\033\\"
```

**Windows Terminal Handling:**
- OSC 8 sequence
- `AddHyperlink()` / `EndHyperlink()` in AdaptDispatch
- Stores hyperlink in TextBuffer attributes
- Renderer underlines hyperlink text
- ControlInteractivity handles Ctrl+Click

### Alternative Screen Buffer

```bash
# Switch to alternate screen buffer
echo -e "\033[?1049h"

# Switch back to main screen buffer
echo -e "\033[?1049l"
```

Used by applications like `vim`, `less`, `htop` to preserve screen contents when they exit.

**Windows Terminal Handling:**
- Private mode sequence (DECSET/DECRST with parameter 1049)
- `SetMode()` / `ResetMode()` in AdaptDispatch
- Terminal maintains two separate TextBuffer instances
- Switches active buffer

---

## The State Machine

### Implementation

**File:** `src/terminal/parser/stateMachine.cpp`

The StateMachine is the heart of VT sequence parsing. It processes input character by character, maintaining state and accumulating parameters.

### Key Design Features

1. **Character-by-Character Processing**
   ```cpp
   void StateMachine::ProcessString(const std::wstring_view string)
   {
       for (const auto wch : string)
       {
           ProcessCharacter(wch);
       }
   }
   ```

2. **State Transitions**
   Based on current state and input character, transitions to new state and executes actions.

3. **Parameter Accumulation**
   ```cpp
   // For ESC[10;5H (move to row 10, col 5)
   // Accumulates parameters: [10, 5]
   std::vector<VTParameter> _parameters;
   ```

4. **Limits**
   ```cpp
   constexpr VTInt MAX_PARAMETER_VALUE = 65535;
   constexpr size_t MAX_PARAMETER_COUNT = 32;
   constexpr size_t MAX_SUBPARAMETER_COUNT = 6;
   ```

### Example State Transitions

**Input:** `"Hello\x1b[31mWorld"`

```
┌──────────┬───────────┬──────────────────────────────────────┐
│ Input    │ State     │ Action                               │
├──────────┼───────────┼──────────────────────────────────────┤
│ 'H'      │ Ground    │ _ActionPrint('H')                    │
│ 'e'      │ Ground    │ _ActionPrint('e')                    │
│ 'l'      │ Ground    │ _ActionPrint('l')                    │
│ 'l'      │ Ground    │ _ActionPrint('l')                    │
│ 'o'      │ Ground    │ _ActionPrint('o')                    │
│ ESC      │ Ground    │ _EnterEscape()                       │
│ '['      │ Escape    │ _EnterCsiEntry()                     │
│ '3'      │ CsiEntry  │ _ActionParam('3')                    │
│ '1'      │ CsiParam  │ _ActionParam('1') → param = 31       │
│ 'm'      │ CsiParam  │ _ActionCsiDispatch('m')              │
│          │           │   → Dispatch SGR(31)                 │
│          │           │   → _EnterGround()                   │
│ 'W'      │ Ground    │ _ActionPrint('W') [with red color]   │
│ 'o'      │ Ground    │ _ActionPrint('o') [with red color]   │
│ 'r'      │ Ground    │ _ActionPrint('r') [with red color]   │
│ 'l'      │ Ground    │ _ActionPrint('l') [with red color]   │
│ 'd'      │ Ground    │ _ActionPrint('d') [with red color]   │
└──────────┴───────────┴──────────────────────────────────────┘
```

### Why a State Machine?

VT sequences can be **complex** and **interleaved**:
- Parameters can be multi-digit numbers
- Multiple parameters separated by semicolons
- Different sequence types have different formats
- Invalid sequences must be handled gracefully
- Partial sequences might span multiple input buffers

The state machine approach allows **incremental parsing** without requiring lookahead or buffering entire sequences.

---

## Code Flow Through Windows Terminal

### Complete Flow: Process Output → Screen

```cpp
// 1. Process writes VT sequence to stdout
//    Application code: printf("\033[31mRed Text\033[0m");

// 2. ConPTY captures output
//    Windows Console Host translates to VT sequences if needed

// 3. ConptyConnection reads from pipe
// File: src/cascadia/TerminalConnection/ConptyConnection.cpp:732
DWORD ConptyConnection::_OutputThread()
{
    char buffer[128 * 1024];
    DWORD read = 0;

    // Continuous read loop
    ReadFile(_pipe.get(), &buffer[0], sizeof(buffer), &read, &overlapped);

    // Convert UTF-8 to UTF-16 and fire event
    TerminalOutput.raise(winrt_wstring_to_array_view(wstr));
}

// 4. ControlCore receives output
// File: src/cascadia/TerminalControl/ControlCore.cpp
void ControlCore::_outputHandler(const array_view<wchar_t> data)
{
    // Forward to Terminal
    _terminal->Write(data);
}

// 5. Terminal processes input
// File: src/cascadia/TerminalCore/Terminal.cpp
void Terminal::Write(std::wstring_view stringView)
{
    // Use state machine to parse
    _stateMachine->ProcessString(stringView);
}

// 6. StateMachine parses sequences
// File: src/terminal/parser/stateMachine.cpp
void StateMachine::ProcessString(const std::wstring_view string)
{
    for (const auto wch : string)
    {
        // State machine processes each character
        _EventGround(wch);  // or other state handler
    }
}

// 7. StateMachine dispatches commands
// File: src/terminal/parser/stateMachine.cpp
void StateMachine::_ActionCsiDispatch(const wchar_t wch)
{
    // Route to engine based on command
    _engine->ActionCsiDispatch(id, _parameters);
}

// 8. OutputStateMachineEngine routes to AdaptDispatch
// File: src/terminal/parser/OutputStateMachineEngine.cpp
bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id, const VTParameters parameters)
{
    switch (id)
    {
    case CsiActionCodes::SGR_SetGraphicsRendition:
        return _dispatch->SetGraphicsRendition(parameters);
    case CsiActionCodes::CUP_CursorPosition:
        return _dispatch->CursorPosition(parameters.at(0), parameters.at(1));
    // ... many more cases
    }
}

// 9. AdaptDispatch executes command
// File: src/terminal/adapter/adaptDispatch.cpp
void AdaptDispatch::SetGraphicsRendition(const VTParameters options)
{
    // Parse SGR parameters and update attributes
    for (const auto option : options)
    {
        switch (option)
        {
        case 31: // Red foreground
            _api.SetTextForegroundIndex(TextColor::DARK_RED);
            break;
        case 0: // Reset
            _api.SetDefaultAttributes();
            break;
        // ... many more cases
        }
    }
}

// 10. Terminal updates TextBuffer
// File: src/cascadia/TerminalCore/Terminal.cpp
void Terminal::SetTextForegroundIndex(const BYTE index)
{
    _textBuffer->GetCursor().GetAttributes().SetIndexedForeground(index);
}

// 11. Renderer triggered
// File: src/renderer/base/renderer.cpp
void Renderer::TriggerRedraw()
{
    // Mark dirty regions
    _srViewportDirty = viewport;

    // On next frame, poll Terminal and render
}

// 12. AtlasEngine renders to screen
// File: src/renderer/atlas/AtlasEngine.cpp
HRESULT AtlasEngine::PaintBufferLine(span<Cluster> clusters, til::point coord, ...)
{
    // For each cluster:
    // - Look up glyph in texture atlas
    // - Apply foreground/background colors
    // - Build vertex buffer
    // - Render with DirectX
}

// 13. SwapChain presents frame
// User sees updated screen with red text
```

### Key Files in VT Processing

| File | Role |
|------|------|
| `src/terminal/parser/stateMachine.hpp` | State machine definition |
| `src/terminal/parser/stateMachine.cpp` | State machine implementation |
| `src/terminal/parser/OutputStateMachineEngine.cpp` | Routes parsed sequences |
| `src/terminal/adapter/adaptDispatch.hpp` | Command interface |
| `src/terminal/adapter/adaptDispatch.cpp` | Command execution |
| `src/cascadia/TerminalCore/Terminal.cpp` | Terminal state management |
| `src/types/inc/colorTable.hpp` | Color definitions |

---

## Practical Examples

### Example 1: Colored Output in C

```c
#include <stdio.h>

int main() {
    // Print "Hello" in default color
    printf("Hello ");

    // Switch to red foreground
    printf("\033[31m");
    printf("World");

    // Reset colors
    printf("\033[0m");
    printf(" Again\n");

    return 0;
}
```

**Output:** `Hello` (normal) `World` (red) `Again` (normal)

**VT Sequences Used:**
- `\033[31m` - Set foreground to red
- `\033[0m` - Reset all attributes

### Example 2: Progress Bar

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Progress: [");
    for (int i = 0; i < 50; i++) {
        // Print one '#' character in green
        printf("\033[32m#\033[0m");
        fflush(stdout);
        usleep(50000);  // 50ms delay
    }
    printf("]\n");
    return 0;
}
```

**VT Sequences Used:**
- `\033[32m` - Set foreground to green
- `\033[0m` - Reset

### Example 3: Cursor Positioning

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    // Clear screen
    printf("\033[2J");

    // Draw a box at position 10,10
    printf("\033[10;10H+-----+");
    printf("\033[11;10H|     |");
    printf("\033[12;10H|     |");
    printf("\033[13;10H+-----+");

    // Put a message in the middle of the box
    printf("\033[12;12HHi!");

    // Move cursor to bottom of screen
    printf("\033[25;1H");

    return 0;
}
```

**VT Sequences Used:**
- `\033[2J` - Clear entire screen
- `\033[10;10H` - Move cursor to row 10, column 10
- `\033[11;10H` - Move cursor to row 11, column 10
- etc.

### Example 4: Spinning Animation

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    const char spinner[] = {'|', '/', '-', '\\'};

    printf("Loading: ");
    fflush(stdout);

    for (int i = 0; i < 50; i++) {
        // Print spinner character
        printf("%c", spinner[i % 4]);
        fflush(stdout);

        usleep(100000);  // 100ms

        // Move cursor back one position
        printf("\033[1D");
    }

    printf("Done!\n");
    return 0;
}
```

**VT Sequences Used:**
- `\033[1D` - Move cursor back (left) 1 position

### Example 5: Interactive Menu (Python)

```python
import sys

def clear_screen():
    sys.stdout.write("\033[2J\033[H")

def move_cursor(row, col):
    sys.stdout.write(f"\033[{row};{col}H")

def set_color(fg, bg=None):
    if bg:
        sys.stdout.write(f"\033[{fg};{bg}m")
    else:
        sys.stdout.write(f"\033[{fg}m")

def reset_color():
    sys.stdout.write("\033[0m")

def draw_menu():
    clear_screen()

    move_cursor(5, 20)
    set_color(33, 44)  # Yellow on blue
    print("═════════════════════")

    move_cursor(6, 20)
    print("║   MAIN MENU      ║")

    move_cursor(7, 20)
    print("═════════════════════")

    reset_color()

    move_cursor(9, 20)
    set_color(32)  # Green
    print("1. Option One")

    move_cursor(10, 20)
    print("2. Option Two")

    move_cursor(11, 20)
    set_color(31)  # Red
    print("3. Exit")

    reset_color()
    move_cursor(13, 20)
    print("Select: ", end='')
    sys.stdout.flush()

draw_menu()
```

**VT Sequences Used:**
- `\033[2J` - Clear screen
- `\033[H` - Move cursor to home
- `\033[row;colH` - Position cursor
- `\033[33;44m` - Yellow foreground, blue background
- `\033[32m` - Green foreground
- `\033[31m` - Red foreground
- `\033[0m` - Reset

---

## Writing Programs That Use VT Sequences

### Enabling VT Processing in Windows

On Windows, you need to enable VT processing for console output:

**C/C++:**
```c
#include <windows.h>

void EnableVTMode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

int main() {
    EnableVTMode();
    printf("\033[31mRed Text\033[0m\n");
    return 0;
}
```

**Note:** Windows Terminal automatically enables VT processing, so this is mainly needed for legacy console applications.

### Best Practices

1. **Always Reset Attributes**
   ```c
   printf("\033[31mRed");
   printf("\033[0m");  // ← Important! Reset before exit
   ```

2. **Use Terminfo/Termcap Libraries**
   - ncurses (C/C++)
   - blessed (Python)
   - termion (Rust)
   - colorama (Python, cross-platform)

3. **Check Terminal Capabilities**
   ```bash
   # Check if terminal supports colors
   if [ -t 1 ]; then
       # stdout is a terminal
       echo -e "\033[31mColored\033[0m"
   fi
   ```

4. **Flush Output When Needed**
   ```c
   printf("\033[31mRed");
   fflush(stdout);  // Force output immediately
   ```

5. **Handle Alternate Screen Buffer**
   ```c
   // Enter alternate screen
   printf("\033[?1049h");

   // Your TUI application
   // ...

   // Exit alternate screen (restore previous content)
   printf("\033[?1049l");
   ```

### Common Libraries

**C/C++:**
- **ncurses** - Full-featured TUI library
- **termbox** - Minimalist TUI library
- **fmt** (C++) - Modern formatting with color support

**Python:**
- **colorama** - Cross-platform colored output
- **rich** - Rich text and formatting
- **blessed** - Terminal manipulation
- **curses** - Python bindings for ncurses

**Rust:**
- **termion** - Terminal manipulation
- **crossterm** - Cross-platform terminal library
- **tui-rs** - Terminal UI framework

**JavaScript/Node.js:**
- **chalk** - Terminal string styling
- **blessed** - TUI framework
- **ansi-colors** - ANSI color codes

---

## Advanced Topics

### Sub-Parameters

Modern VT sequences support sub-parameters separated by colons:

```
ESC[38:5:208m      ← 256-color foreground (orange)
     └┬┘ └──┴── Sub-parameters
      └── Main parameter
```

**Windows Terminal Support:**
```cpp
// src/terminal/parser/stateMachine.hpp:35
constexpr size_t MAX_SUBPARAMETER_COUNT = 6;
```

### Private Mode Sequences

Private modes use `?` after the CSI:

```
ESC[?1049h    ← Enable alternate screen buffer
    └─ Private mode indicator
```

**Windows Terminal Handling:**
```cpp
// Different dispatch path for private modes
case DECSET_PrivateModeSet:  // ESC[?...h
case DECRST_PrivateModeReset: // ESC[?...l
```

### DEC Private Modes

| Mode | Description |
|------|-------------|
| 1 | Application cursor keys |
| 3 | 132 column mode |
| 25 | Show cursor |
| 47 | Alternate screen buffer |
| 1000 | Mouse tracking (X11) |
| 1002 | Button event mouse tracking |
| 1004 | Focus in/out events |
| 1006 | SGR mouse mode |
| 1049 | Alternate screen + cursor save |
| 2004 | Bracketed paste mode |

### Bracketed Paste Mode

Prevents pasted text from being interpreted as commands:

```bash
# Enable
echo -e "\033[?2004h"

# When text is pasted, it's wrapped:
# ESC[200~ <pasted text> ESC[201~

# Disable
echo -e "\033[?2004l"
```

**Windows Terminal Support:** Full support in ConptyConnection

---

## Debugging VT Sequences

### Viewing Raw Sequences

**Bash:**
```bash
# Use cat -v to show control characters
echo -e "\033[31mRed\033[0m" | cat -v
# Output: ^[[31mRed^[[0m

# Use od (octal dump)
echo -e "\033[31m" | od -An -tx1
# Output: 1b 5b 33 31 6d
```

**PowerShell:**
```powershell
# View bytes
[System.Text.Encoding]::UTF8.GetBytes("`e[31m") | Format-Hex
```

### Logging in Windows Terminal

**Enable VT Parsing Tracing:**

Edit Windows Terminal settings and add to a profile:
```json
{
    "experimental.connection.passthroughMode": true
}
```

Or use Windows Terminal's built-in logging (developer builds).

### Common Issues

1. **Sequences Not Working**
   - Check VT processing is enabled
   - Verify terminal emulator support
   - Check for typos in sequence

2. **Colors Look Wrong**
   - Check color scheme in terminal settings
   - Verify 256-color or RGB support
   - Some terminals may not support true color

3. **Cursor Positioning Off**
   - Remember: rows and columns are 1-based
   - Check terminal window size
   - Verify cursor position sequences

---

## References

### Standards and Specifications

- **ECMA-48** - Control Functions for Coded Character Sets
- **DEC VT100** - Original VT100 documentation
- **XTerm Control Sequences** - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
- **Console Virtual Terminal Sequences** - Microsoft Docs

### Windows Terminal Source Files

| File | Description |
|------|-------------|
| `src/terminal/parser/stateMachine.hpp` | State machine header |
| `src/terminal/parser/stateMachine.cpp` | State machine implementation |
| `src/terminal/parser/OutputStateMachineEngine.hpp` | VT sequence action codes |
| `src/terminal/parser/OutputStateMachineEngine.cpp` | Sequence routing |
| `src/terminal/adapter/adaptDispatch.hpp` | Command interface |
| `src/terminal/adapter/adaptDispatch.cpp` | Command implementation |
| `src/terminal/adapter/termDispatch.hpp` | Base dispatch interface |

### Online Resources

- **VT100.net** - http://vt100.net - Historical VT terminal documentation
- **XTerm FAQ** - https://invisible-island.net/xterm/xterm.faq.html
- **ANSI Escape Sequences Wiki** - Wikipedia article on ANSI escape codes
- **Terminal Guide** - https://terminalguide.namepad.de/ - Modern terminal guide

---

## Summary

**VT sequences** are the language that terminal applications use to control terminal behavior. In Windows Terminal:

1. **Applications generate VT sequences** as text output
2. **ConPTY forwards** them to Windows Terminal
3. **StateMachine parses** sequences character by character
4. **AdaptDispatch executes** the parsed commands
5. **Terminal updates** its internal state (TextBuffer, cursor, colors)
6. **Renderer draws** the final visual output to screen

Understanding VT sequences is crucial for:
- Building terminal applications (TUIs)
- Debugging terminal output issues
- Understanding how terminal emulators work
- Creating colored console output
- Implementing custom terminal features

Windows Terminal implements comprehensive VT sequence support, making it compatible with thousands of existing terminal applications while adding modern extensions like hyperlinks, RGB colors, and SIXEL graphics.

---

**Document Version:** 1.0
**Last Updated:** 2026-01-26
**Windows Terminal Repository:** https://github.com/microsoft/terminal
