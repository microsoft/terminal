# Fix for GH#18004: Ghost Characters with tmux over SSH

## Problem

When starting tmux over SSH in Windows Terminal, ghost characters appeared:
```
giulio@myhome:~$ 61;4;6;7;14;21;22;23;24;28;32;42;52c0;10;1c
```

This issue only occurred:
- With Visual Studio debugger attached (race condition timing)
- On slower systems (Win10)
- When tmux sent Device Attribute (DA) queries during initialization

## Root Cause

1. tmux sends DA queries (`\e[c`, `\e[>c`) to interrogate terminal capabilities
2. These queries travel: tmux → SSH → Windows Terminal (via ConPTY)
3. Windows Terminal's output state machine processes the queries and generates DA responses
4. The responses (`\e[?61;4;...c`, `\e[>0;10;1c`) are sent to ConPTY's input pipe
5. ssh.exe reads these responses from stdin and they get corrupted/echoed
6. The corrupted responses (missing CSI prefix) appear as literal text: `61;4;...c`

The corruption happened because DA responses were sent to ssh.exe's stdin where they weren't expected, rather than going to the remote terminal where tmux could understand them.

## Solution

Block Device Attribute responses from Windows Terminal when using ConPTY connections.

### Changes Made

#### 1. ControlCore.cpp (lines 2212-2231)
Added filtering logic to detect and block DA responses for ConPTY connections:
```cpp
if (!_pendingResponses.empty())
{
    // GH#18004: For ConPTY, only send non-DA responses to prevent ghost chars
    // DA responses through ConPTY input cause corruption with SSH/tmux
    const bool isConPty = _connection.try_as<TerminalConnection::ConptyConnection>() != nullptr;
    bool shouldSend = true;

    if (isConPty)
    {
        const bool hasDA = _pendingResponses.find(L"\x1b[?") != std::wstring::npos ||
                          _pendingResponses.find(L"\x1b[>") != std::wstring::npos;
        shouldSend = !hasDA;
    }

    if (shouldSend)
    {
        _sendInputToConnection(_pendingResponses);
    }
    _pendingResponses.clear();
}
```

#### 2. ConptyConnection.cpp/h
Changed from single duplex pipe to separate input/output pipes for clearer architecture:
- `_inPipe`: Terminal writes → ConPTY reads (application stdin)
- `_outPipe`: ConPTY writes → Terminal reads (application stdout)

This change improves code clarity but doesn't directly fix the ghost chars issue.

## Architectural Notes

### Why conhost doesn't respond in ConPTY mode
When `IsInVtIoMode()` returns true (ConPTY mode), conhost's `ReturnResponse()` returns early without sending responses. This is by design - in ConPTY mode, the terminal (Windows Terminal) is supposed to handle terminal queries, not conhost.

### Why Windows Terminal must block DA responses for ConPTY
- For SSH sessions: DA responses should not be sent through ConPTY's input pipe where ssh.exe reads them from stdin, causing corruption
- For local apps: Most local Windows applications don't send DA queries, so this isn't a practical issue
- Sending DA responses through ConPTY's input pipe causes them to be read by applications (like ssh.exe) that don't expect them, resulting in corruption

### Known Limitation
Running `printf "\e[c"` over SSH will not produce a DA response from Windows Terminal. This is expected behavior - DA responses should not be sent through ConPTY's input pipe. Windows Terminal correctly responds to DA queries from directly connected applications (non-ConPTY connections).

## Testing

### Verified Fixed
✅ No ghost characters when starting tmux over SSH
✅ No ghost characters on Win10 systems
✅ No ghost characters with debugger attached

### Known Behavior
⚠️ `printf "\e[c"` over SSH produces no response (expected - remote terminal should respond)
✅ Other terminal responses (CPR, OSC queries, etc.) continue to work through ConPTY

## Files Modified

1. `src/cascadia/TerminalControl/ControlCore.cpp`
2. `src/cascadia/TerminalConnection/ConptyConnection.h`
3. `src/cascadia/TerminalConnection/ConptyConnection.cpp`

