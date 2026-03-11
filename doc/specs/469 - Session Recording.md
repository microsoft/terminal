---
author: SemperFu
created on: 2025-01-15
last updated: 2026-03-10
issue id: #469
---

# Session Recording and Playback (asciicast v2/v3)

## Abstract

Windows Terminal supports recording terminal sessions to files and playing them
back using the [asciicast] format for interoperability with the [asciinema]
ecosystem. The recorder writes [asciicast v3] (interval-based timing) and the
player supports both [asciicast v2] and v3. Recording subscribes to the
connection's `TerminalOutput` event to capture raw VT output, while playback
uses a new connection type that replays events with their original timing.

## Inspiration

The original feature request ([#469]) has been one of the most-requested
capabilities since the early days of Windows Terminal. In the issue thread,
[@DHowett noted][dhowett-comment] that the asciicast format maps naturally onto
the Terminal's connection model: recording is simply intercepting
`TerminalOutput` events, and playback is a connection that emits them on a
timer.

The [asciinema] project has established asciicast as the de facto standard for
terminal recordings. By adopting this format, Windows Terminal gains immediate
compatibility with:

- The `asciinema` CLI recorder/player
- The [asciinema-player] web component
- [asciinema.org] hosting and sharing
- Third-party tools like `svg-term`, `agg`, and others

[#469]: https://github.com/microsoft/terminal/issues/469
[dhowett-comment]: https://github.com/microsoft/terminal/issues/469
[asciinema]: https://asciinema.org
[asciinema-player]: https://github.com/asciinema/asciinema-player
[asciinema.org]: https://asciinema.org
[asciicast]: https://docs.asciinema.org/manual/asciicast/v3/
[asciicast v2]: https://docs.asciinema.org/manual/asciicast/v2/
[asciicast v3]: https://docs.asciinema.org/manual/asciicast/v3/

## Solution Design

### Architecture Overview

The design introduces two new components that plug into the existing connection
infrastructure:

```
┌───────────────┐        ┌───────────────────┐
│  ControlCore  │────────│ AsciicastRecorder │
│               │        │  subscribes to    │
│               │        │  TerminalOutput   │
└───────────────┘        └───────────────────┘
                                 │
                                 ▼
                          .cast file on disk

┌──────────────┐         ┌─────────────────────┐        ┌──────────┐
│  Terminal    │◄────────│ AsciicastConnection │◄───────│ .cast    │
│  Control     │ Output  │ (playback)          │        │ file     │
│              │         │                     │        │          │
└──────────────┘         └─────────────────────┘        └──────────┘
```

### Recording: `AsciicastRecorder`

`AsciicastRecorder` is a plain C++ class owned by `ControlCore`. It subscribes
to the active connection's `TerminalOutput` event to capture raw VT data and
writes it to a `.cast` file with interval-based timing (v3 format).

```cpp
class AsciicastRecorder
{
public:
    void StartRecording(const ITerminalConnection& connection,
                        const std::wstring& filePath,
                        uint32_t width, uint32_t height);
    void StopRecording();
    void WriteInitialSnapshot(const std::wstring_view& data);
    bool IsRecording() const noexcept;

private:
    ITerminalConnection _connection{ nullptr };
    std::ofstream _file;
    std::chrono::high_resolution_clock::time_point _lastEventTime;
    mutable std::mutex _mutex;
    bool _isRecording{ false };
    winrt::event_token _outputToken{};

    void _WriteHeader(uint32_t width, uint32_t height);
    void _WriteEvent(const std::wstring_view& data);
    void _WriteExitEvent();
    static std::string _EscapeJsonString(const std::wstring_view& input);
};
```

**Key behaviors:**

- On `StartRecording()`, opens the output file and writes the asciicast v3
  header with terminal dimensions and a Unix timestamp.
- Subscribes to the connection's `TerminalOutput` event. Each chunk is
  written as an `"o"` (output) event with the interval since the previous event.
- `WriteInitialSnapshot()` captures the current prompt line with VT attributes
  (colors, bold, etc.) as the first event so recordings don't start blank.
- On `StopRecording()`, writes an exit event, revokes the event subscription,
  and closes the file.
- All write operations are guarded by `std::mutex` for thread safety.
- Each event is flushed immediately so a crash preserves most of the recording.

### Playback: `AsciicastConnection`

`AsciicastConnection` implements `ITerminalConnection` and replays a `.cast`
file by emitting `TerminalOutput` events with the original timing.

```cpp
class AsciicastConnection : public BaseTerminalConnection<AsciicastConnection>
{
public:
    void Initialize(const ValueSet& settings);
    void Start();
    void WriteInput(const array_view<const char16_t> buffer);
    void Resize(uint32_t rows, uint32_t columns) noexcept;
    void Close() noexcept;

private:
    winrt::hstring _filePath;
    std::vector<CastEvent> _events;
    int _formatVersion{ 2 };
    std::atomic<bool> _cancelled{ false };
    std::atomic<bool> _playbackFinished{ false };

    void _parseFile();
    winrt::fire_and_forget _replayEvents();
    static std::wstring _unescapeJsonString(std::wstring_view input);
};
```

**Key behaviors:**

- On `Start()`, parses the header line to detect v2 vs v3, then parses all
  event lines into memory.
- Events are replayed as a `fire_and_forget` coroutine with `resume_after`
  delays. For v2 files (absolute timestamps), the delay is the delta between
  consecutive events. For v3 files (interval timestamps), the delay is used
  directly.
- After playback, the connection moves the cursor to the bottom of the viewport
  and shows "Playback complete. Press any key to close." Any subsequent input
  closes the connection.
- Supports `#` comment lines (v3 feature) and skips malformed lines gracefully.

### Asciicast v3 Format (recorded)

[Asciicast v3][asciicast v3] uses newline-delimited JSON with interval-based
timing:

```jsonc
// Line 1: Header object
{"version": 3, "term": {"cols": 120, "rows": 30}, "timestamp": 1709999999}
// Lines 2+: Event tuples [interval, type, data]
[0.000, "o", "PS C:\\Users\\user> "]
[1.500, "o", "Get-Process"]
[0.020, "o", "\r\n"]
// Last line: Exit event
[2.000, "x", "0"]
```

Key differences from v2: timestamps are intervals (time since previous event)
rather than absolute offsets, the header uses `term.cols`/`term.rows` instead
of top-level `width`/`height`, and an exit event `"x"` marks the end.

### Asciicast v2 Format (playback only)

[Asciicast v2][asciicast v2] uses absolute timestamps:

```jsonc
{"version": 2, "width": 120, "height": 30, "timestamp": 1709999999}
[0.000000, "o", "Windows PowerShell\r\n"]
[0.120000, "o", "PS C:\\Users\\user> "]
[1.500000, "o", "Get-Process"]
```

The player auto-detects v2 vs v3 from the `version` field in the header and
adjusts its timing calculations accordingly.

## UI/UX Design

### Starting a Recording

1. The user presses `Ctrl+Shift+R`, selects "Record session" from the tab
   context menu, or uses the command palette.
2. A Save As dialog opens with a default filename:
   `Terminal Recording YYYY-MM-DD HHmmss.cast`
3. A red dot indicator appears in the tab header while recording.
4. The current prompt line is captured with VT color attributes as the first
   recorded event.
5. All subsequent VT output is written with interval timing.

### Stopping a Recording

The user presses `Ctrl+Shift+R` again, or selects "Stop recording" from the tab
context menu. The recorder writes an exit event and closes the file.

Recording also stops when the tab is closed or the connection exits.

### Playing Back a Recording

1. The user presses `Ctrl+Shift+O` or selects "Open cast file" from the
   command palette.
2. A file-open dialog appears, filtered to `*.cast` files.
3. A new tab opens with the session replaying at original speed.
4. When playback completes, the tab shows "Playback complete. Press any key to
   close." and waits for input before closing.

## Capabilities

### Actions

| Action | Command ID | Default Keybinding | Description |
|--------|-----------|-------------------|-------------|
| Toggle recording | `toggleRecording` | `Ctrl+Shift+R` | Opens Save As dialog on start; stops and finalizes on subsequent press. |
| Open cast file | `openCastFile` | `Ctrl+Shift+O` | Opens a file picker and plays the selected `.cast` file in a new tab. |

### Tab UI

| Element | Description |
|---------|-------------|
| Red dot indicator | Shown in tab header during recording |
| Context menu | "Record session" / "Stop recording" (toggles based on state) |
| Command palette | Both actions listed |

### Accessibility

- Playback tabs are standard terminal tabs. All existing accessibility
  features (screen reader, high contrast, zoom) apply to replayed content.

### Security

- **Sensitive data**: Terminal recordings capture all visible output, which may
  include passwords (if echoed), tokens, API keys, and other secrets.
- **File permissions**: The `.cast` file inherits the permissions of the
  target directory chosen in the Save As dialog.
- **No network transmission**: Recording and playback are entirely local
  operations. Sharing is an explicit user action (copy the file).

### Reliability

- If the recording file cannot be created (permissions, disk full), the action
  fails silently and recording does not start.
- If a `.cast` file is malformed, the parser skips invalid lines and replays
  whatever events it can.
- The recorder flushes after every event so that a crash preserves most of the
  recording.

### Compatibility

- The recorder writes asciicast v3. The player reads both v2 and v3.
- Recordings made by `asciinema rec` (v2) play back in Windows Terminal.
- Recordings made by Windows Terminal (v3) play back with the `asciinema` CLI,
  `asciinema-player` web component, and `agg` GIF generator.
- This feature does not change the behavior of any existing connections,
  keybindings, or settings.

### Performance

- **Recording overhead**: Each output event requires a JSON serialization and a
  synchronous file write + flush. For typical interactive sessions this adds
  negligible latency.
- **File sizes**: A typical 30-minute interactive session produces roughly
  100 KB to 1 MB. High-throughput sessions can produce tens of MB.
- **Playback**: Events are loaded into memory on start. Even long sessions
  consume only a few MB of RAM.

## Potential Issues

1. **Large recordings**: Sessions with heavy output (e.g., build logs) can
   produce large `.cast` files. A configurable size limit could be added later.

2. **Terminal size mismatch**: If the playback terminal is smaller than the
   recorded dimensions, content may reflow or clip.

3. **Sensitive data**: Users may inadvertently record secrets. A future
   improvement could add a warning when recording starts.

4. **Encoding edge cases**: Binary output or invalid UTF-8 from the connection
   is escaped or dropped during JSON serialization.

## Future Considerations

- **Input recording**: Add `"i"` (input) events to record keystrokes. Requires
  careful handling of security implications.

- **Playback controls**: Pause, resume, speed multiplier, seek.

- **Export**: Convert `.cast` to GIF or SVG via command palette action.

- **Sharing**: Upload to [asciinema.org] and copy URL to clipboard.

- **CLI recording**: `wt record --output session.cast -- pwsh`

- **Compression**: Support `.cast.gz` for long sessions.

- **File association**: Register `.cast` so double-clicking opens playback.

- **Snippet recording**: Ring buffer mode, save last N seconds on demand.

## Resources

- [asciicast v3 format specification](https://docs.asciinema.org/manual/asciicast/v3/)
- [asciicast v2 format specification](https://docs.asciinema.org/manual/asciicast/v2/)
- [asciinema project](https://asciinema.org)
- [asciinema-player web component](https://github.com/asciinema/asciinema-player)
- [Windows Terminal issue #469](https://github.com/microsoft/terminal/issues/469)
- [agg (asciicast GIF generator)](https://github.com/asciinema/agg)
