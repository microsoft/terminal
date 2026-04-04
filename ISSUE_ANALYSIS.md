# Square Brackets in Path - Issue Analysis

## Problem
When launching PowerShell through Windows Terminal's "Open in Terminal" context menu from a folder path containing square brackets (e.g., `C:\Users\BlueRain\Downloads\Windows 11 [DVD]\`), PowerShell loses the current path information and defaults to `C:\Windows\System32\WindowsPowerShell\v1.0`.

## Root Cause
The issue occurs in `src/cascadia/ShellExtension/OpenTerminalHere.cpp` where the path is passed to Windows Terminal in two ways:

1. As a command-line argument: `-d "C:\Path\With\[Brackets]"`
2. As the `lpCurrentDirectory` parameter to `CreateProcessW`

When Windows Terminal receives the `-d` parameter and passes it to PowerShell via ConPTY, PowerShell interprets square brackets `[` and `]` as wildcard characters (similar to glob patterns). When the wildcard pattern doesn't match any existing path, PowerShell falls back to its default directory.

## Related Issues
- PowerShell/PowerShell#5752 - PowerShell does not open in current directory if its name contains square brackets
- microsoft/vscode#70432 - VSCode has the same issue
- microsoft/vscode#148574 - VSCode terminal with square brackets

## Proposed Solutions

### Solution 1: Escape Square Brackets in QuoteAndEscapeCommandlineArg
Modify the `QuoteAndEscapeCommandlineArg` function in `src/cascadia/WinRTUtils/inc/WtExeUtils.h` to escape square brackets with backticks for PowerShell compatibility.

**Pros:**
- Fixes the issue at the source
- Works for all shells that might have issues with square brackets

**Cons:**
- The escaping is shell-specific (PowerShell uses backticks, bash uses backslashes)
- May break other shells that don't expect escaped brackets

### Solution 2: Use Literal Path Syntax
Wrap the path in PowerShell's literal path syntax when passing to PowerShell specifically.

**Pros:**
- PowerShell-specific solution
- Clean and idiomatic

**Cons:**
- Requires detecting which shell is being launched
- More complex implementation

### Solution 3: Rely Only on lpCurrentDirectory
Remove the `-d` parameter and rely solely on the `lpCurrentDirectory` parameter of `CreateProcessW`.

**Pros:**
- Simplest solution
- The OS handles the path correctly

**Cons:**
- The `-d` parameter might be needed for other purposes
- Need to verify this doesn't break other functionality

## Recommended Approach
**Solution 1** - Escape square brackets in the `QuoteAndEscapeCommandlineArg` function by adding backtick escaping for `[` and `]` characters.

This is the most straightforward fix that will work for PowerShell (both 5.1 and 7+) without breaking other functionality.
