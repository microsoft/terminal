# Quick Start Guide - Square Brackets Fix

## TL;DR
Fixed PowerShell not opening in the correct directory when the path has square brackets.

## What You Need to Know

### The Bug
```
Folder: C:\Downloads\Windows 11 [DVD]\
Action: Right-click → "Open in Terminal"
Result: PowerShell opens in C:\Windows\System32\WindowsPowerShell\v1.0 ❌
Expected: PowerShell opens in C:\Downloads\Windows 11 [DVD]\ ✅
```

### The Fix
One function modified: `QuoteAndEscapeCommandlineArg` in `WtExeUtils.h`
- Added: Escape `[` and `]` with backticks
- Why: PowerShell treats brackets as wildcards
- Impact: ~10 lines of code

### Files Changed
```
Modified: src/cascadia/WinRTUtils/inc/WtExeUtils.h
Added:    src/cascadia/LocalTests_TerminalApp/QuoteAndEscapeTest.cpp
```

## Quick Test

### Manual Test (5 minutes)
```powershell
# 1. Create test folder
mkdir "C:\Test [Brackets]"

# 2. Open in Explorer
explorer "C:\Test [Brackets]"

# 3. Right-click → "Open in Terminal"

# 4. Check location
Get-Location
# Should show: C:\Test [Brackets]
```

### Build & Test
```powershell
# Build
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild

# Test
Invoke-OpenConsoleTests
```

## Submit Your Contribution

### 1. Fork & Branch
```bash
git clone https://github.com/YOUR-USERNAME/terminal.git
cd terminal
git checkout -b fix/square-brackets-in-path
```

### 2. Make Changes
Already done! Files are ready in your workspace.

### 3. Commit
```bash
git add src/cascadia/WinRTUtils/inc/WtExeUtils.h
git add src/cascadia/LocalTests_TerminalApp/QuoteAndEscapeTest.cpp
git commit -m "Fix PowerShell working directory with square brackets in path

- Escape square brackets with backticks in QuoteAndEscapeCommandlineArg
- Add unit tests for square bracket escaping
- Fixes issue where PowerShell fails to set correct working directory
  when path contains square brackets"
```

### 4. Push & PR
```bash
git push origin fix/square-brackets-in-path
```
Then create PR on GitHub with title:
**"Fix PowerShell working directory with square brackets in path"**

## PR Checklist
- [ ] Code builds successfully
- [ ] Tests pass
- [ ] Manual testing completed
- [ ] PR description includes issue reference
- [ ] Screenshots/evidence attached

## Need Help?
- Build issues: See `doc/building.md`
- Contributing: See `CONTRIBUTING.md`
- Detailed docs: See `FIX_DOCUMENTATION.md`

## That's It!
You've fixed a real bug that affects users. Great contribution! 🎉
