---
author: (contributor) @(github-id)
created on: 2025-01-15
last updated: 2025-01-15
issue id: #469
---

# Session Recording and Playback (asciicast v2)

## Abstract

Windows Terminal should support recording terminal sessions to files and playing
them back, using the [asciicast v2] format for broad interoperability with the
[asciinema] ecosystem. Recording wraps an existing `ITerminalConnection` to
capture timestamped VT output, while playback uses a new connection type that
replays events with their original timing. This enables users to create
shareable, lightweight, text-based recordings of their terminal sessions without
third-party tools.

## Inspiration

The original feature request ([#469]) has been one of the most-requested
capabilities since the early days of Windows Terminal. In the issue thread,
[@DHowett noted][dhowett-comment] that the asciicast format maps naturally onto
the Terminal's connection model — recording is simply intercepting
`TerminalOutput` events, and playback is a connection that emits them on a
timer.

The [asciinema] project has established asciicast as the de facto standard for
terminal recordings. By adopting v2 of this format, Windows Terminal gains
immediate compatibility with:

- The `asciinema` CLI recorder/player
- The [asciinema-player] web component
- [asciinema.org] hosting and sharing
- Third-party tools like `svg-term`, `termtosvg`, and `agg`

[#469]: https://github.com/microsoft/terminal/issues/469
[dhowett-comment]: https://github.com/microsoft/terminal/issues/469
[asciinema]: https://asciinema.org
[asciinema-player]: https://github.com/asciinema/asciinema-player
[asciinema.org]: https://asciinema.org
[asciicast v2]: https://docs.asciinema.org/manual/asciicast/v2/

## Solution Design

### Architecture Overview

The design introduces two new components that plug into the existing connection
infrastructure:

```
┌──────────────┐         ┌───────────────────┐        ┌──────────┐
│  Terminal     │◄────────│ AsciicastRecorder  │◄───────│ ConPTY / │
│  Control      │ Output  │ (decorator)        │ Output │ SSH /    │
│               │─────────│                    │────────│ Azure    │
│               │ Input   │                    │ Input  │          │
└──────────────┘         └───────────────────┘        └──────────┘
                                 │
                                 ▼
                          .cast file on disk

┌──────────────┐         ┌───────────────────┐        ┌──────────┐
│  Terminal     │◄────────│ AsciicastConnection│◄───────│ .cast    │
│  Control      │ Output  │ (playback)         │        │ file     │
│               │         │                    │        │          │
└──────────────┘         └───────────────────┘        └──────────┘
```

### Recording: `AsciicastRecorder`

`AsciicastRecorder` is a decorator that wraps any `ITerminalConnection`. It
transparently forwards input and output while capturing timestamped output
events to a `.cast` file.

```cpp
class AsciicastRecorder : public ITerminalConnection
{
public:
    AsciicastRecorder(ITerminalConnection innerConnection,
                      std::filesystem::path outputPath,
                      uint16_t columns,
                      uint16_t rows);

    // ITerminalConnection implementation — delegates to inner connection
    void Start() override;
    void WriteInput(hstring data) override;
    void Resize(uint32_t rows, uint32_t columns) override;
    void Close() override;

    // Recording control
    void StartRecording();
    void StopRecording();
    bool IsRecording() const;

private:
    ITerminalConnection _inner;
    std::ofstream _file;
    std::chrono::steady_clock::time_point _startTime;
    bool _recording{ false };

    void _onOutput(hstring data);   // subscribed to _inner.TerminalOutput
    void _writeEvent(double elapsed, std::string_view type, std::string_view data);
};
```

**Key behaviors:**

- On `StartRecording()`, opens the output file and writes the asciicast v2
  header line containing the terminal dimensions and a Unix timestamp.
- Subscribes to the inner connection's `TerminalOutput` event. Each chunk is
  written as an `"o"` (output) event with the elapsed time since recording
  began.
- Input from the user is forwarded to the inner connection unchanged. Input is
  **not** recorded by default (see [Future Considerations](#future-considerations)).
- On `StopRecording()`, flushes and closes the file.
- File writes are buffered and performed asynchronously to avoid blocking the
  output pipeline.

### Playback: `AsciicastConnection`

`AsciicastConnection` implements `ITerminalConnection` and replays a `.cast`
file by emitting `TerminalOutput` events with the original inter-event timing.

```cpp
class AsciicastConnection : public ITerminalConnection
{
public:
    AsciicastConnection(std::filesystem::path castFile);

    void Start() override;          // begins playback
    void WriteInput(hstring) override;  // ignored (or future: control commands)
    void Resize(uint32_t, uint32_t) override;  // ignored
    void Close() override;          // stops playback

    // Metadata from cast header
    uint16_t Width() const;
    uint16_t Height() const;

private:
    std::filesystem::path _path;
    std::vector<CastEvent> _events;
    size_t _currentEvent{ 0 };
    ThreadpoolTimer _timer;

    void _scheduleNextEvent();
    void _emitOutput(hstring data);
};
```

**Key behaviors:**

- On `Start()`, parses the header line to extract `width`, `height`, and other
  metadata, then parses all event lines into memory.
- Events are replayed using a threadpool timer. After emitting one event, the
  next timer is scheduled for `(next_elapsed - current_elapsed)` seconds.
- The tab hosting this connection should be sized to the `width` × `height`
  from the header to match the original recording dimensions.
- When all events have been emitted, the connection fires `StateChanged` with
  a `Closed` state.

### Asciicast v2 Format

The [asciicast v2 format][asciicast v2] is NDJSON (newline-delimited JSON):

```jsonc
// Line 1: Header object
{"version": 2, "width": 120, "height": 30, "timestamp": 1709999999, "env": {"SHELL": "pwsh", "TERM": "xterm-256color"}}
// Lines 2+: Event tuples
[0.000000, "o", "Windows PowerShell\r\n"]
[0.120000, "o", "PS C:\\Users\\user> "]
[1.500000, "o", "Get-Process"]
[1.520000, "o", "\r\n"]
```

| Field | Description |
|-------|-------------|
| `version` | Always `2` |
| `width` | Terminal column count at recording start |
| `height` | Terminal row count at recording start |
| `timestamp` | Unix epoch seconds when recording started |
| `env` | Optional environment metadata (`SHELL`, `TERM`) |

Event tuples:

| Index | Type | Description |
|-------|------|-------------|
| 0 | `float` | Seconds elapsed since recording start |
| 1 | `string` | Event type: `"o"` = output, `"i"` = input |
| 2 | `string` | The data payload (UTF-8, JSON-escaped) |

Windows Terminal will write `"o"` events only. `"i"` events are reserved for
future input recording support.

## UI/UX Design

### Starting a Recording

1. The user presses **Ctrl+Shift+R** or selects **"Toggle Session Recording"**
   from the command palette.
2. A red recording indicator (●) appears in the active tab's title area, to the
   left of the tab title. The indicator pulses subtly to convey "recording in
   progress."
3. All VT output from the connection is captured to a timestamped `.cast` file
   in the configured recording directory.
4. The user presses **Ctrl+Shift+R** again (or uses the command palette) to
   stop recording.
5. A toast notification appears with the saved file path and a clickable link
   to open the containing folder.

### Stopping a Recording

Recording also stops automatically when:
- The tab is closed
- The connection exits
- The terminal window is closed

In all cases the `.cast` file is finalized (flushed and closed) before the
connection tears down.

### Playing Back a Recording

1. The user selects **"Open Terminal Recording..."** from the command palette.
2. A standard file-open dialog appears, filtered to `*.cast` files.
3. A new tab opens with the session replaying at original speed. The tab title
   is set to the filename (e.g., `session-2025-01-15.cast`).
4. The tab's terminal dimensions are set to match the `width` and `height`
   from the `.cast` header.
5. When playback completes, the tab remains open (showing final output) with a
   status bar message: "Playback complete."

### File Naming

Recordings are saved with an auto-generated name:

```
Terminal Recording YYYY-MM-DD HHmmss.cast
```

Example: `Terminal Recording 2025-01-15 143022.cast`

## Capabilities

### Settings

| Setting | Key | Type | Default | Description |
|---------|-----|------|---------|-------------|
| Default recording directory | `recording.defaultDirectory` | `string` | `%USERPROFILE%\Videos\Terminal Recordings` | Filesystem path where `.cast` files are saved. Environment variables are expanded. |

### Actions

| Action | Command Name | Default Keybinding | Description |
|--------|-------------|-------------------|-------------|
| Toggle recording | `toggleRecording` | `ctrl+shift+r` | Starts recording if not active; stops and saves if already recording. |
| Open recording | `openRecording` | _(none)_ | Opens a file picker for `.cast` files and plays the selected file in a new tab. |

### Accessibility

- The recording indicator is exposed to UI Automation as a status element so
  screen readers can announce "Recording started" / "Recording stopped."
- The toast notification on save is accessible via the Windows notification
  system.
- Playback tabs are standard terminal tabs — all existing accessibility
  features (screen reader, high contrast, zoom) apply to replayed content.

### Security

- **Sensitive data**: Terminal recordings capture all visible output, which may
  include passwords (if echoed), tokens, API keys, and other secrets. The save
  notification should include a reminder: _"This recording may contain sensitive
  information."_
- **File permissions**: On save, the `.cast` file inherits the permissions of
  the target directory. The default directory is under the user's profile, so
  files are user-accessible only by default.
- **No network transmission**: Recording and playback are entirely local
  operations. Sharing is an explicit user action (copy the file).

### Reliability

- If the recording file cannot be created (permissions, disk full), the
  `toggleRecording` action fails gracefully with a user-facing error toast, and
  the session continues unrecorded.
- If a `.cast` file is malformed or truncated, `AsciicastConnection` should
  display a clear error message in the tab rather than crashing.
- The recorder flushes periodically (not just on stop) so that a crash
  preserves most of the recording.

### Compatibility

- The asciicast v2 format is well-established and unlikely to change in
  breaking ways. If a v3 format is released, a new reader can be added
  alongside the v2 reader.
- This feature does not change the behavior of any existing connections,
  keybindings, or settings. The `Ctrl+Shift+R` binding is new and does not
  conflict with any current default binding.
- Recordings made by `asciinema rec` can be played back in Windows Terminal,
  and recordings made by Windows Terminal can be played back with the
  `asciinema` CLI or web player.

### Performance, Power, and Efficiency

- **Recording overhead**: Each output event requires a JSON serialization and a
  buffered file write. For typical interactive sessions, this adds negligible
  latency. For high-throughput scenarios (e.g., `cat`-ing a large file), the
  buffered async writer prevents blocking the render loop.
- **File sizes**: A typical 30-minute interactive session produces roughly
  100 KB–1 MB. High-throughput sessions (continuous output) can produce tens
  of MB. Compression is deferred to a future iteration.
- **Playback**: Events are loaded into memory on start. A one-hour session
  with heavy output might consume a few MB of RAM — negligible on modern
  systems.

## Potential Issues

1. **Large recordings**: Sessions with heavy output (e.g., build logs, large
   file dumps) can produce very large `.cast` files. Mitigation: document
   expected sizes; consider a configurable maximum recording duration or file
   size limit in the future.

2. **Terminal size mismatch on playback**: If the user's terminal is smaller
   than the recorded dimensions, content may reflow or be clipped. Mitigation:
   resize the playback tab to match the header's `width` × `height`, and
   display the original dimensions in the tab tooltip.

3. **Sensitive data exposure**: Users may inadvertently record passwords,
   tokens, or other secrets. Mitigation: display a clear warning when recording
   starts and when a file is saved.

4. **Concurrent recordings**: What happens if the user starts recording on a
   tab that is already recording? Mitigation: `toggleRecording` stops the
   existing recording before starting a new one (i.e., it truly toggles).

5. **Encoding edge cases**: The asciicast format requires UTF-8 JSON-encoded
   strings. Binary output or invalid UTF-8 sequences from the connection must
   be handled (e.g., by escaping or dropping invalid bytes).

6. **Connection lifecycle**: If the inner connection closes while recording,
   the recorder must finalize the file cleanly. The `StateChanged` event from
   the inner connection triggers `StopRecording()`.

## Future Considerations

- **Input recording**: Add `"i"` (input) events to record what the user typed.
  This enables full session replay (not just output). Requires careful
  consideration of security implications (keystrokes may include passwords).

- **Playback controls**: Pause, resume, speed multiplier (0.5×, 1×, 2×, 5×),
  and seek-to-timestamp. These could be exposed as keybindings active only in
  playback tabs or via an on-screen control bar.

- **Export**: Convert `.cast` files to GIF or SVG using bundled or external
  tools. This could be a command palette action: "Export Recording as GIF..."

- **Sharing**: Integration with [asciinema.org] or other hosting services.
  A "Share Recording" action could upload the `.cast` file and copy the URL
  to the clipboard.

- **CLI recording**: Allow recording to be initiated from the command line:
  ```
  wt record --output session.cast -- pwsh
  ```
  This would launch a new terminal tab with the recorder already active.

- **Compression**: For long or high-throughput sessions, support gzip
  compression of `.cast` files (`.cast.gz`), which asciinema tools already
  understand.

- **File association**: Register `.cast` as a file type handled by Windows
  Terminal, so double-clicking a `.cast` file opens it in a playback tab.

- **Snippet recording**: Record only the last N seconds (ring buffer mode)
  and save on demand — useful for capturing unexpected behavior after the fact.

## Resources

- [asciicast v2 format specification](https://docs.asciinema.org/manual/asciicast/v2/)
- [asciinema project](https://asciinema.org)
- [asciinema-player web component](https://github.com/asciinema/asciinema-player)
- [Windows Terminal issue #469](https://github.com/microsoft/terminal/issues/469)
- [agg — asciinema GIF generator](https://github.com/asciinema/agg)
- [svg-term-cli — render asciicast to SVG](https://github.com/marionebl/svg-term-cli)
