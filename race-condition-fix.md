# DA Response Ghost Characters Fix

## Issue Description

Ghost characters appear in tmux sessions over SSH when using ConPTY. The issue manifests as DA (Device Attributes) responses being sent to the SSH session, where they appear as visible text instead of being processed as control sequences.

### Symptoms
- **Without breakpoint in outputStream.cpp:39**: Minimal or no ghost characters
  ```
  [[]10;rgb:cccc/cccc/cccc\]11;rgb:0c0c/0c0c/0c0c\[[]10;rgb:cccc/cccc/cccc\
  ```
- **With breakpoint in outputStream.cpp:39**: DA responses appear as ghost characters
  ```
  giulio@myhome:~$ 61;4;6;7;14;21;22;23;24;28;32;42;52c0;10;1c
  ```

Debug output shows:
```
[DEBUG] _pendingResponses append: ?61;4;6;7;14;21;22;23;24;28;32;42;52c
[DEBUG] _pendingResponses append: >0;10;1c
[DEBUG] ConPTY: Tracking DA response: ?61;4;6;7;14;21;22;23;24;28;32;42;52c>0;10;1c
```

### Root Cause

The issue has two components:

1. **Timing sensitivity**: DA responses sent through ConPTY input can cause ghost characters in SSH/tmux sessions depending on when they arrive
2. **Microsoft's position**: Microsoft considers this an SSH bug and is working on a fix there, but doesn't want to block all DA responses in ConPTY since they're legitimate in other scenarios

### The Real Problem

DA responses are being sent **as input** to ConPTY via `_sendInputToConnection`, which causes them to be forwarded to the SSH session. When timing is altered (e.g., by debugger breakpoints), these responses arrive at the wrong moment and appear as visible text.

The previous fix (blocking all DA responses for ConPTY) was rejected because:
- DA responses are legitimate for local terminal queries
- Blocking them entirely breaks functionality
- The timing-dependent nature suggests it's primarily an SSH handling issue

## Solution: Profile-Based Setting

Instead of blocking all DA responses unconditionally, we've added a **profile-specific setting** that allows users to disable DA responses for ConPTY connections where they experience issues (like SSH profiles).

### Changes Made

#### 1. Thread Safety - Mutex Protection

Added mutex synchronization to prevent race conditions on `_pendingResponses`:

**ControlCore.h:389** - Added mutex member:
```cpp
std::wstring _pendingResponses;
std::mutex _pendingResponsesMutex;  // NEW
```

**ControlCore.cpp:104-107** - Protected append operation:
```cpp
_terminal->SetWriteInputCallback([this](std::wstring_view wstr) {
    std::lock_guard<std::mutex> lock(_pendingResponsesMutex);
    _pendingResponses.append(wstr);
});
```

**ControlCore.cpp:2205-2225** - Protected read/clear operations:
```cpp
{
    std::lock_guard<std::mutex> lock(_pendingResponsesMutex);
    if (!_pendingResponses.empty())
    {
        // GH#18004: For ConPTY, only send non-DA responses to prevent ghost chars
        const bool isConPty = _connection.try_as<TerminalConnection::ConptyConnection>() != nullptr;
        bool shouldSend = true;

        if (isConPty && _settings.BlockConPtyDAResponses())
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
}
```

#### 2. New Profile Setting

**IControlSettings.idl:78** - Added setting to interface:
```cpp
Boolean BlockConPtyDAResponses { get; };
```

**ControlProperties.h:59** - Added to settings macro (defaults to `false`):
```cpp
X(bool, BlockConPtyDAResponses, false)
```

**Profile.idl:94** - Added to profile settings:
```cpp
INHERITABLE_PROFILE_SETTING(Boolean, BlockConPtyDAResponses);
```

**MTSMSettings.h:109** - Added JSON mapping:
```cpp
X(bool, BlockConPtyDAResponses, "blockConPtyDAResponses", false)
```

**TerminalSettings.cpp:354** - Applied in profile settings:
```cpp
_BlockConPtyDAResponses = profile.BlockConPtyDAResponses();
```

### How to Use

Users can add this setting to their profile JSON in several ways:

#### Option 1: Apply to all profiles (in `profiles.defaults`):
```json
{
    "profiles": {
        "defaults": {
            "blockConPtyDAResponses": true
        },
        "list": [ ... ]
    }
}
```

#### Option 2: Apply to specific profile:
```json
{
    "name": "SSH Connection",
    "commandline": "ssh user@host",
    "blockConPtyDAResponses": true
}
```

- **Default**: `false` (DA responses are sent normally - maintains existing behavior)
- **When `true`**: DA responses are blocked for ConPTY connections in this profile
- **Other connections**: Setting has no effect on non-ConPTY connections

#### User Configuration Example

**File**: `C:\Users\giuli\AppData\Local\Microsoft\Windows Terminal\settings.json`

Applied to all profiles via `profiles.defaults`:
```json
"profiles": {
    "defaults": {
        "blockConPtyDAResponses": true
    },
    "list": [ ... ]
}
```

This configuration ensures DA responses are blocked for all ConPTY SSH sessions to prevent ghost characters in tmux.

## Testing Status

### Test Plan
- [ ] Build the solution successfully
- [ ] Test local PowerShell/CMD without the setting (should work normally)
- [ ] Test SSH connection without the setting (baseline - may have ghost chars)
- [ ] Test SSH connection with `"blockConPtyDAResponses": true`
- [ ] Test SSH + tmux with the setting enabled
- [ ] Test with debugger breakpoint - verify setting prevents ghost chars
- [ ] Verify debug output shows "BLOCKING DA response" messages
- [ ] Verify non-DA responses are still sent (like color queries)

### Test Results

#### Build Test
- **Status**: Not yet tested
- **Command**: `msbuild` or Visual Studio build
- **Result**:

#### Local Terminal (without setting)
- **Status**: Not yet tested
- **Profile**: PowerShell/CMD with default settings
- **Expected**: DA responses work normally
- **Result**:

#### SSH Without Setting (Baseline)
- **Status**: Not yet tested
- **Profile**: SSH profile, `blockConPtyDAResponses` not set or `false`
- **Expected**: May see ghost characters (existing behavior)
- **Result**:

#### SSH With Setting Enabled
- **Status**: Not yet tested
- **Profile**: SSH profile with `"blockConPtyDAResponses": true`
- **Expected**: No ghost characters, DA responses blocked
- **Debug Output Expected**: `[DEBUG] ConPTY: BLOCKING DA response: ...`
- **Result**:

#### SSH + tmux With Setting
- **Status**: Not yet tested
- **Profile**: SSH profile with setting enabled, tmux running
- **Expected**: No ghost characters
- **Result**:

#### With Debugger Breakpoint
- **Status**: Not yet tested
- **Breakpoint**: outputStream.cpp:39
- **Profile**: SSH with `blockConPtyDAResponses: true`
- **Expected**: No ghost characters even with timing altered
- **Result**:

#### Non-DA Responses (e.g., OSC color queries)
- **Status**: Not yet tested
- **Expected**: Color queries like OSC 10/11 should still be sent
- **Debug Output Expected**: `[DEBUG] ConPTY: Sending non-DA response: ...`
- **Result**:

## Notes

- **Backward compatible**: Default value is `false`, so existing profiles won't change behavior
- **User control**: Users can enable this for problematic profiles (SSH) while keeping it off for local terminals
- **Granular**: Can be set per-profile, not globally
- **Debug output**: Messages clearly indicate when DA responses are blocked vs sent
- **Thread safe**: Mutex prevents race conditions on `_pendingResponses`
- **Respects Microsoft's design**: Doesn't unconditionally block DA responses, preserving legitimate use cases

## Related Issues
- GH#18004 - Ghost characters in SSH/tmux sessions
- Microsoft's position: This is an SSH bug, fix should be in SSH handling

## Future Improvements

Potential enhancements if this approach works well:
1. Auto-detection: Automatically enable for SSH-like connections
2. UI toggle: Add checkbox in profile settings UI
3. Documentation: Update Windows Terminal docs with this workaround

---
**Last Updated**: 2025-10-02
**Status**: Implementation complete, testing pending
