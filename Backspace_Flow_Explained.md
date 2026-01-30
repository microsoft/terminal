# Backspace Key Flow in Windows Terminal

> A detailed walkthrough of what happens when you press Backspace in PowerShell running in Windows Terminal
>
> Generated: 2026-01-26

## Table of Contents

1. [Overview](#overview)
2. [The Complete Flow](#the-complete-flow)
3. [Input Path: Keyboard to PowerShell](#input-path-keyboard-to-powershell)
4. [PowerShell Processing](#powershell-processing)
5. [Output Path: PowerShell to Screen](#output-path-powershell-to-screen)
6. [Code-Level Details](#code-level-details)
7. [Why Two Paths?](#why-two-paths)
8. [Comparing with Direct Terminal Applications](#comparing-with-direct-terminal-applications)

---

## Overview

When you press Backspace while typing in PowerShell, the character doesn't disappear instantly. Instead, a complex round-trip happens:

```
User Presses Backspace
        ↓
Terminal Sends Character to PowerShell (INPUT PATH)
        ↓
PowerShell Processes and Updates Its Line Buffer
        ↓
PowerShell Sends VT Sequences Back (OUTPUT PATH)
        ↓
Terminal Parses VT Sequences and Updates Display
        ↓
Screen Updates - Character Disappears
```

**Key Insight:** The terminal itself doesn't erase the character. It sends a signal to PowerShell, which then tells the terminal how to update the display.

---

## The Complete Flow

### High-Level Overview

```
┌──────────────────────────────────────────────────────────────────┐
│  1. USER ACTION                                                  │
│  User presses Backspace key on keyboard                          │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  2. WINDOWS EVENT                                                │
│  Windows OS generates WM_KEYDOWN message                         │
│  wParam = VK_BACK (0x08)                                        │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  3. WINDOWS TERMINAL INPUT PROCESSING                            │
│  IslandWindow::OnKeyDown() → TermControl → ControlInteractivity │
│  → ControlCore → Terminal                                        │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  4. INPUT TRANSLATION                                            │
│  TerminalInput::HandleKey() converts VK_BACK to:                │
│  • Default: \x7F (DEL character, ASCII 127)                     │
│  • Or: \b (BS character, ASCII 8) if BackarrowKey mode on      │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  5. SEND TO POWERSHELL                                           │
│  ConptyConnection::WriteInput() writes to pipe                   │
│  PowerShell receives the character via ConPTY                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     │ === CROSSES PROCESS BOUNDARY ===
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  6. POWERSHELL PROCESSING                                        │
│  • PSReadLine (PowerShell's line editor) receives \x7F          │
│  • Detects it as "delete previous character"                    │
│  • Removes last char from internal line buffer                  │
│  • Calculates what needs to change on screen                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  7. POWERSHELL GENERATES OUTPUT                                  │
│  PSReadLine writes VT sequences to stdout:                       │
│  • ESC[D  - Move cursor left 1 position                         │
│  • ESC[K  - Erase from cursor to end of line                    │
│  (Or other sequences depending on situation)                     │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     │ === CROSSES PROCESS BOUNDARY ===
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  8. CONPTY CAPTURES OUTPUT                                       │
│  Windows ConPTY intercepts PowerShell's stdout                   │
│  Forwards VT sequences through pipe to Terminal                  │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  9. TERMINAL RECEIVES OUTPUT                                     │
│  ConptyConnection::_OutputThread() reads from pipe               │
│  Fires TerminalOutput event with VT sequences                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  10. VT SEQUENCE PARSING                                         │
│  StateMachine::ProcessString() parses ESC[D and ESC[K            │
│  Routes to OutputStateMachineEngine                              │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  11. EXECUTE TERMINAL COMMANDS                                   │
│  AdaptDispatch executes:                                         │
│  • CursorBackward(1) - Move cursor left                         │
│  • EraseInLine(ToEnd) - Clear to end of line                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  12. UPDATE TERMINAL BUFFER                                      │
│  Terminal updates TextBuffer:                                    │
│  • Cursor position moved left                                   │
│  • Character cell(s) cleared                                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  13. RENDER TO SCREEN                                            │
│  Renderer::TriggerRedraw()                                       │
│  AtlasEngine draws updated frame                                 │
│  SwapChain presents to screen                                    │
└────────────────────┬─────────────────────────────────────────────┘
                     │
                     ↓
┌──────────────────────────────────────────────────────────────────┐
│  14. USER SEES RESULT                                            │
│  Character disappears from screen                                │
│  Cursor is now one position to the left                          │
└──────────────────────────────────────────────────────────────────┘
```

---

## Input Path: Keyboard to PowerShell

### Step-by-Step Code Flow

#### 1. Windows Sends Key Event

Windows generates a `WM_KEYDOWN` message when you press Backspace:
- `wParam = VK_BACK` (virtual key code 0x08)
- `lParam` contains scan code and other flags

#### 2. IslandWindow Captures Event

**File:** `src/cascadia/WindowsTerminal/IslandWindow.cpp`

```cpp
// IslandWindow receives WM_KEYDOWN from Windows
LRESULT IslandWindow::MessageHandler(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN)
    {
        // wParam = VK_BACK (0x08)
        // Routes to TermControl
    }
}
```

#### 3. XAML Event Routing

**Flow:** `IslandWindow` → XAML Island → `TermControl` (XAML control) → Key event handlers

#### 4. ControlInteractivity Handles Input

**File:** `src/cascadia/TerminalControl/ControlInteractivity.cpp`

```cpp
void ControlInteractivity::OnKeyDown(KeyRoutedEventArgs const& e)
{
    // VirtualKey = VirtualKey::Back
    // Builds INPUT_RECORD with KEY_EVENT

    INPUT_RECORD inputRecord;
    inputRecord.EventType = KEY_EVENT;
    inputRecord.Event.KeyEvent.wVirtualKeyCode = VK_BACK;
    inputRecord.Event.KeyEvent.bKeyDown = TRUE;
    inputRecord.Event.KeyEvent.uChar.UnicodeChar = 0x08; // Or 0x7F

    // Send to ControlCore
    _core->SendInput(inputRecord);
}
```

#### 5. ControlCore Forwards to Terminal

**File:** `src/cascadia/TerminalControl/ControlCore.cpp`

```cpp
void ControlCore::SendInput(std::wstring_view wstr)
{
    // Forward to Terminal
    _terminal->SendKeyEvent(...);
}
```

#### 6. Terminal Converts to VT Sequence

**File:** `src/cascadia/TerminalCore/Terminal.cpp`

```cpp
bool Terminal::SendKeyEvent(const WORD vkey, const WORD scanCode,
                           const ControlKeyStates states, const bool keyDown)
{
    // Delegates to TerminalInput
    return _terminalInput->HandleKey(inputRecord);
}
```

#### 7. TerminalInput Translates Key

**File:** `src/terminal/input/terminalInput.cpp` (lines 105-231)

```cpp
TerminalInput::OutputType TerminalInput::HandleKey(const INPUT_RECORD& event)
{
    const auto keyEvent = event.Event.KeyEvent;
    const auto virtualKeyCode = keyEvent.wVirtualKeyCode; // VK_BACK

    // Build lookup key with modifiers
    auto keyCombo = virtualKeyCode;
    // (Apply Ctrl, Alt, Shift modifiers if present)

    // Look up in pre-built key map
    const auto keyMatch = _keyMap.find(keyCombo);
    if (keyMatch != _keyMap.end())
    {
        return keyMatch->second; // Returns the mapped string
    }

    // If not in map, use unicode char
    // ...
}
```

**Key Map Initialization** (lines 373-382):

```cpp
void TerminalInput::_initKeyboardMap()
{
    // BACKSPACE maps to either DEL or BS, depending on Backarrow Key mode
    // Mode is controlled by DECBKM (DEC Backarrow Key Mode)

    const auto backSequence = _inputMode.test(Mode::BackarrowKey)
        ? L"\b"s      // BS (0x08) if BackarrowKey mode ON
        : L"\x7F"s;   // DEL (0x7F) if BackarrowKey mode OFF (default)

    const auto ctrlBackSequence = _inputMode.test(Mode::BackarrowKey)
        ? L"\x7F"s    // DEL if mode ON
        : L"\b"s;     // BS if mode OFF

    // Map VK_BACK to appropriate sequence
    defineKeyWithAltModifier(VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + VK_BACK, ctrlBackSequence);
    defineKeyWithAltModifier(Shift + VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + Shift + VK_BACK, ctrlBackSequence);
}
```

**Default Mapping:**
- Backspace → `\x7F` (DEL character, ASCII 127)
- Ctrl+Backspace → `\b` (BS character, ASCII 8)

**Historical Note:**
- **BS (0x08, \b)**: Original ASCII backspace - move cursor left one position
- **DEL (0x7F, \x7F)**: Delete character at cursor
- Most modern shells expect DEL (0x7F) for backspace
- This is why Windows Terminal defaults to sending DEL

#### 8. Send to Connection

**File:** `src/cascadia/TerminalCore/Terminal.cpp`

```cpp
bool Terminal::SendKeyEvent(...)
{
    auto result = _terminalInput->HandleKey(inputRecord);
    if (result.has_value())
    {
        // result = L"\x7F"
        // Write to connection (PowerShell)
        _connection.WriteInput(*result);
    }
}
```

#### 9. Write to ConPTY Pipe

**File:** `src/cascadia/TerminalConnection/ConptyConnection.cpp` (line 548)

```cpp
void ConptyConnection::WriteInput(const winrt::array_view<const char16_t> buffer)
{
    // buffer = L"\x7F"

    // Convert UTF-16 to UTF-8
    std::string str = til::u16u8(buffer);
    // str = "\x7F" (single byte in UTF-8)

    // Acquire write lock
    const auto lock = _writeLock.lock();

    // Write to pipe using overlapped I/O
    WriteFile(_pipe.get(), str.data(), str.size(), ...);
}
```

#### 10. ConPTY Delivers to PowerShell

Windows ConPTY (Pseudoconsole) infrastructure:
- Receives the byte `0x7F` from the pipe
- Delivers it to PowerShell's stdin as if user typed it
- PowerShell sees it as normal input

---

## PowerShell Processing

### What PowerShell Does with Backspace

PowerShell uses **PSReadLine** module for command-line editing. When it receives `0x7F` (DEL):

#### 1. PSReadLine Receives Input

```csharp
// Simplified PSReadLine logic (actual code is in C#)

void ProcessKey(ConsoleKeyInfo key)
{
    // key.KeyChar = 0x7F (DEL)
    // key.Key = ConsoleKey.Backspace

    if (key.Key == ConsoleKey.Backspace)
    {
        HandleBackspace();
    }
}
```

#### 2. Update Internal Buffer

```csharp
void HandleBackspace()
{
    if (_currentCursorPosition > 0)
    {
        // Remove character from line buffer
        _buffer.RemoveAt(_currentCursorPosition - 1);
        _currentCursorPosition--;

        // Calculate screen update needed
        RenderLine();
    }
}
```

#### 3. Generate VT Sequences for Display Update

PSReadLine calculates the most efficient way to update the screen:

**Simple Case** (cursor at end of line):
```
Before: "Hello World█"  (cursor after 'd')
After:  "Hello Worl█"   (cursor after 'l')
```

PowerShell writes to stdout:
```
\x1b[D     ← ESC[D  - Move cursor left 1 position
\x1b[K     ← ESC[K  - Erase from cursor to end of line
```

**Complex Case** (cursor in middle of line):
```
Before: "Hello█World"   (cursor after 'o', in middle)
After:  "Hell█World"    (cursor after second 'l')
```

PowerShell might write:
```
\x1b[D     ← Move cursor left
World      ← Rewrite rest of line
\x1b[K     ← Erase any remaining characters
\x1b[5D    ← Move cursor back to correct position
```

#### 4. Write to Stdout

PSReadLine calls Console.Write() or directly writes to stdout:
```csharp
Console.Write("\x1b[D\x1b[K"); // VT sequences
```

This output goes through ConPTY back to Windows Terminal.

---

## Output Path: PowerShell to Screen

### Step-by-Step Code Flow

#### 1. ConPTY Captures Output

Windows ConPTY monitors PowerShell's stdout and captures the VT sequences:
- `\x1b[D` (ESC[D)
- `\x1b[K` (ESC[K)

#### 2. Pipe to Terminal

ConPTY writes the sequences to the output pipe that Windows Terminal is reading from.

#### 3. ConptyConnection Reads Output

**File:** `src/cascadia/TerminalConnection/ConptyConnection.cpp` (line 732)

```cpp
DWORD ConptyConnection::_OutputThread()
{
    // Background thread continuously reading from pipe
    char buffer[128 * 1024];
    DWORD read = 0;

    // Overlapped I/O read
    ReadFile(_pipe.get(), &buffer[0], sizeof(buffer), &read, &overlapped);

    // buffer now contains: "\x1b[D\x1b[K"

    // Convert UTF-8 to UTF-16
    til::u8u16(buffer, read, wstr);

    // Fire event
    TerminalOutput.raise(winrt_wstring_to_array_view(wstr));
}
```

#### 4. ControlCore Receives Output

**File:** `src/cascadia/TerminalControl/ControlCore.cpp`

```cpp
void ControlCore::_outputHandler(const array_view<wchar_t> data)
{
    // data = L"\x1b[D\x1b[K"

    // Forward to Terminal
    _terminal->Write(data);
}
```

#### 5. Terminal Processes Output

**File:** `src/cascadia/TerminalCore/Terminal.cpp`

```cpp
void Terminal::Write(std::wstring_view stringView)
{
    // stringView = L"\x1b[D\x1b[K"

    // Use state machine to parse
    _stateMachine->ProcessString(stringView);
}
```

#### 6. StateMachine Parses VT Sequences

**File:** `src/terminal/parser/stateMachine.cpp`

```cpp
void StateMachine::ProcessString(const std::wstring_view string)
{
    // Processes: "\x1b[D\x1b[K"

    for (const auto wch : string)
    {
        ProcessCharacter(wch);
    }
}
```

**Character-by-character parsing:**

```
┌─────┬───────────┬────────────────────────────────────────┐
│ Char│ State     │ Action                                 │
├─────┼───────────┼────────────────────────────────────────┤
│ ESC │ Ground    │ _EnterEscape()                         │
│ [   │ Escape    │ _EnterCsiEntry()                       │
│ D   │ CsiEntry  │ _ActionCsiDispatch(id='D')             │
│     │           │   → Dispatch CursorBackward(1)         │
│     │           │   → _EnterGround()                     │
│ ESC │ Ground    │ _EnterEscape()                         │
│ [   │ Escape    │ _EnterCsiEntry()                       │
│ K   │ CsiEntry  │ _ActionCsiDispatch(id='K')             │
│     │           │   → Dispatch EraseInLine(ToEnd)        │
│     │           │   → _EnterGround()                     │
└─────┴───────────┴────────────────────────────────────────┘
```

#### 7. OutputStateMachineEngine Routes Commands

**File:** `src/terminal/parser/OutputStateMachineEngine.cpp`

```cpp
bool OutputStateMachineEngine::ActionCsiDispatch(const VTID id,
                                                 const VTParameters parameters)
{
    switch (id)
    {
    case CsiActionCodes::CUB_CursorBackward: // 'D'
        return _dispatch->CursorBackward(parameters.at(0).value_or(1));

    case CsiActionCodes::EL_EraseLine: // 'K'
        return _dispatch->EraseInLine(
            parameters.at(0).value_or(DispatchTypes::EraseType::ToEnd));

    // ... many more cases
    }
}
```

#### 8. AdaptDispatch Executes Commands

**File:** `src/terminal/adapter/adaptDispatch.cpp`

**ESC[D - CursorBackward(1):**
```cpp
void AdaptDispatch::CursorBackward(const VTInt distance)
{
    // distance = 1
    auto cursorPosition = _api.GetCursorPosition();

    // Move cursor left by 1
    cursorPosition.x -= distance;

    // Clamp to valid range
    cursorPosition.x = std::max(0, cursorPosition.x);

    // Update cursor position
    _api.SetCursorPosition(cursorPosition.x, cursorPosition.y);
}
```

**ESC[K - EraseInLine(ToEnd):**
```cpp
void AdaptDispatch::EraseInLine(const DispatchTypes::EraseType eraseType)
{
    // eraseType = ToEnd (default, erase from cursor to end of line)

    auto cursorPosition = _api.GetCursorPosition();
    auto& textBuffer = _api.GetTextBuffer();

    // Get current row
    auto& row = textBuffer.GetRowByOffset(cursorPosition.y);

    // Erase from cursor position to end of line
    for (int x = cursorPosition.x; x < row.size(); x++)
    {
        row.ClearCell(x); // Clear character and attributes
    }

    // Mark row as modified for rendering
    textBuffer.TriggerRedraw(Viewport::FromCoord(cursorPosition));
}
```

#### 9. Terminal Updates TextBuffer

**File:** `src/cascadia/TerminalCore/Terminal.cpp`

```cpp
void Terminal::SetCursorPosition(til::CoordType x, til::CoordType y)
{
    auto& cursor = _textBuffer->GetCursor();
    cursor.SetPosition(til::point{ x, y });
}
```

The TextBuffer now reflects:
- Cursor position moved left by 1
- Character(s) to the right erased
- Buffer marked dirty for rendering

#### 10. Renderer Updates Display

**File:** `src/renderer/base/renderer.cpp`

```cpp
void Renderer::TriggerRedraw()
{
    // Mark viewport dirty
    _srViewportDirty = _pData->GetViewport();

    // On next frame (60 FPS), render
    PaintFrame();
}
```

#### 11. AtlasEngine Renders Frame

**File:** `src/renderer/atlas/AtlasEngine.cpp`

```cpp
HRESULT AtlasEngine::PaintBufferLine(span<Cluster> clusters, ...)
{
    // For each character cluster in the dirty region:
    // - Look up glyph in texture atlas
    // - Build vertex buffer
    // - Apply colors
    // - Render with DirectX

    // The erased character position now renders as blank/background
}
```

#### 12. SwapChain Presents

DirectX presents the frame to the display:
```cpp
_swapChain->Present(1, 0); // Vsync enabled
```

#### 13. User Sees Result

The screen now shows:
- Character removed
- Cursor moved left
- Visual update complete

---

## Code-Level Details

### Key Data Structures

#### INPUT_RECORD

```cpp
// Windows input event structure
typedef struct _INPUT_RECORD {
    WORD EventType;      // KEY_EVENT, MOUSE_EVENT, etc.
    union {
        KEY_EVENT_RECORD KeyEvent;
        MOUSE_EVENT_RECORD MouseEvent;
        // ...
    } Event;
} INPUT_RECORD;

typedef struct _KEY_EVENT_RECORD {
    BOOL bKeyDown;              // TRUE for key press
    WORD wRepeatCount;          // Repeat count
    WORD wVirtualKeyCode;       // VK_BACK = 0x08
    WORD wVirtualScanCode;      // Hardware scan code
    union {
        WCHAR UnicodeChar;      // Unicode character
        CHAR AsciiChar;         // ASCII character
    } uChar;
    DWORD dwControlKeyState;    // Ctrl, Alt, Shift, etc.
} KEY_EVENT_RECORD;
```

#### TextBuffer Cell

```cpp
// Stores one character cell in the terminal buffer
struct CharacterAttributes
{
    uint16_t foreground : 4;   // Foreground color index
    uint16_t background : 4;   // Background color index
    uint16_t bold : 1;         // Bold attribute
    uint16_t italic : 1;       // Italic attribute
    uint16_t underline : 1;    // Underline attribute
    // ... more attributes
};

struct Cell
{
    wchar_t character;         // The character (or space if empty)
    CharacterAttributes attrs; // Visual attributes
};
```

### Timing and Performance

**Total Latency:** Typically 5-30 milliseconds for the complete round-trip

**Breakdown:**
1. Input processing (Windows Terminal): ~1-2 ms
2. Pipe write: <1 ms
3. PowerShell processing: ~2-10 ms (depends on PSReadLine complexity)
4. Pipe read: <1 ms
5. VT parsing: ~1-2 ms
6. Buffer update: <1 ms
7. Rendering (next frame): ~16 ms (60 FPS) or less

**Why you don't notice the latency:**
- Modern systems are fast
- Overlapped I/O prevents blocking
- Rendering is optimized (only dirty regions)
- GPU-accelerated rendering
- Double buffering prevents tearing

---

## Why Two Paths?

### Why Not Just Erase Directly?

You might wonder: "Why doesn't Windows Terminal just erase the character directly when Backspace is pressed?"

**Answer:** Because the terminal is a **display device**, not an **editor**.

#### The Terminal Model

Traditional terminals (like VT100) were hardware devices:
- They displayed what was sent to them
- They forwarded user input to the computer
- They didn't have "intelligence" about what was being typed
- The **application** (shell, editor, etc.) controlled the display

Windows Terminal follows this model for compatibility with thousands of existing applications.

#### Benefits of This Architecture

**1. Application Control**
- PowerShell (or bash, vim, etc.) controls its own line editing
- Different applications can implement different editing behaviors
- Rich line editing features (PSReadLine with auto-completion, syntax highlighting)

**2. Complex Scenarios**
```
Before: "Hello World"  (cursor after 'o', in middle)
Press Backspace
After:  "Hell World"   (not "Hell ", need to shift remaining text)
```

The application knows how to:
- Move remaining text left
- Handle multi-byte characters (emoji, Unicode)
- Apply syntax highlighting
- Show auto-completion
- Implement custom key bindings

**3. Network Transparency**
- SSH sessions work the same way
- Terminal doesn't need to know about remote vs. local
- Same protocol works over any connection

**4. Historical Compatibility**
- Works with legacy applications from 1970s-1980s
- VT100 compatibility
- Standard protocol all applications understand

---

## Comparing with Direct Terminal Applications

### Terminal-Aware Applications (vim, less, htop)

Full-screen applications bypass the shell's line editing:

```
┌──────────────────────────────────────────────────┐
│  vim running in terminal                         │
│                                                  │
│  1. User presses Backspace                       │
│     ↓                                            │
│  2. Terminal sends \x7F to vim                   │
│     ↓                                            │
│  3. vim's input handler processes it             │
│     ↓                                            │
│  4. vim updates its internal buffer              │
│     ↓                                            │
│  5. vim sends VT sequences to redraw screen:     │
│     - Position cursor                            │
│     - Redraw modified lines                      │
│     - Apply syntax highlighting                  │
│     ↓                                            │
│  6. Terminal parses VT sequences                 │
│     ↓                                            │
│  7. Screen updates                               │
└──────────────────────────────────────────────────┘
```

**Same principle:** Application controls the display

### GUI Text Editors (VS Code, Notepad++)

GUI applications don't use VT sequences:

```
┌──────────────────────────────────────────────────┐
│  1. User presses Backspace                       │
│     ↓                                            │
│  2. Windows sends WM_KEYDOWN                     │
│     ↓                                            │
│  3. Application receives keyboard event          │
│     ↓                                            │
│  4. Application updates document                 │
│     ↓                                            │
│  5. Application calls rendering API              │
│     (Direct2D, GDI, etc.)                        │
│     ↓                                            │
│  6. Screen updates                               │
└──────────────────────────────────────────────────┘
```

**Direct rendering:** No intermediary display protocol

---

## Special Cases and Edge Cases

### Multi-Byte Characters

**Emoji or wide characters:**
```
Before: "Hello 👋█"  (emoji is 2 columns wide)
Press Backspace
After:  "Hello █"
```

PowerShell/PSReadLine:
- Detects character width (2 columns)
- Sends VT sequences to erase both columns
- Properly handles Unicode combining characters

### Middle of Line

```
Before: "He█llo"  (cursor after 'e')
Press Backspace
After:  "H█llo"   (cursor after 'H')
```

PSReadLine sends:
```
\x1b[D     ← Move cursor left
\x1b[K     ← Erase to end
llo        ← Rewrite remaining text
\x1b[3D    ← Move cursor back 3 positions
```

### Backspace at Beginning

```
Before: "█Hello"  (cursor at start)
Press Backspace
After:  "█Hello"  (no change)
```

PSReadLine:
- Detects cursor at position 0
- Ignores backspace (or beeps)
- Sends no output

### Ctrl+Backspace (Delete Word)

Many shells support Ctrl+Backspace to delete entire word:

```
Before: "git commit -m '█"
Press Ctrl+Backspace
After:  "git commit █"
```

Terminal sends: `\b` (or `\x7F` depending on mode)
PSReadLine:
- Recognizes it as "delete word backward"
- Removes " -m "
- Redraws line appropriately

---

## Summary

### The Complete Round-Trip

```
KEYBOARD
   ↓
WINDOWS OS (WM_KEYDOWN with VK_BACK)
   ↓
WINDOWS TERMINAL
   ├─ IslandWindow receives Windows message
   ├─ TermControl XAML routing
   ├─ ControlInteractivity handles key
   ├─ ControlCore coordinates
   ├─ Terminal::SendKeyEvent()
   └─ TerminalInput::HandleKey()
      └─ Returns "\x7F" (DEL character)
   ↓
CONNECTION (ConptyConnection)
   └─ WriteInput() writes to pipe
   ↓
═══ PROCESS BOUNDARY ═══
   ↓
WINDOWS CONPTY
   └─ Delivers input to PowerShell stdin
   ↓
POWERSHELL PROCESS
   ├─ PSReadLine receives character
   ├─ Detects backspace (DEL = 0x7F)
   ├─ Removes character from line buffer
   ├─ Calculates screen update
   └─ Writes VT sequences to stdout:
      • ESC[D  (move cursor left)
      • ESC[K  (erase to end of line)
   ↓
WINDOWS CONPTY
   └─ Captures stdout and forwards to pipe
   ↓
═══ PROCESS BOUNDARY ═══
   ↓
CONNECTION (ConptyConnection)
   └─ _OutputThread() reads from pipe
   └─ Fires TerminalOutput event
   ↓
WINDOWS TERMINAL
   ├─ ControlCore receives output
   ├─ Terminal::Write()
   ├─ StateMachine::ProcessString()
   │  └─ Parses ESC[D and ESC[K
   ├─ AdaptDispatch::Execute()
   │  ├─ CursorBackward(1)
   │  └─ EraseInLine(ToEnd)
   ├─ TextBuffer updated
   │  ├─ Cursor moved left
   │  └─ Character erased
   ├─ Renderer::TriggerRedraw()
   └─ AtlasEngine renders
      └─ DirectX presents frame
   ↓
SCREEN
   └─ User sees character disappear
```

### Key Takeaways

1. **Backspace is a round-trip operation**, not a local operation
2. **Terminal sends input** (usually `\x7F` DEL character)
3. **Application processes** and decides what to do
4. **Application sends VT sequences** to update display
5. **Terminal parses and executes** the VT commands
6. **Renderer updates** the visual display

This architecture:
- ✅ Provides application control over editing
- ✅ Enables rich line editing features
- ✅ Maintains compatibility with all terminal applications
- ✅ Works transparently over network (SSH)
- ✅ Follows historical terminal model

The same flow applies to most keys (arrow keys, delete, etc.) - the terminal is a **display and input forwarding device**, not an editor.

---

**Document Version:** 1.0
**Last Updated:** 2026-01-26
**Windows Terminal Repository:** https://github.com/microsoft/terminal
