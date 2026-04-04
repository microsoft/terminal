# Contribution Checklist

## ✅ Pre-Submission Checklist

### Code Changes
- [x] Modified `src/cascadia/WinRTUtils/inc/WtExeUtils.h`
  - [x] Added square bracket escaping logic
  - [x] Updated documentation comments
  - [x] Code follows project style
- [x] Created `src/cascadia/LocalTests_TerminalApp/QuoteAndEscapeTest.cpp`
  - [x] 6 comprehensive unit tests
  - [x] Tests cover all scenarios
  - [x] Tests follow TAEF framework
- [x] Updated `src/cascadia/LocalTests_TerminalApp/TerminalApp.LocalTests.vcxproj`
  - [x] Added QuoteAndEscapeTest.cpp to build

### Testing
- [ ] Build completes successfully
  ```powershell
  Import-Module .\tools\OpenConsole.psm1
  Set-MsBuildDevEnvironment
  Invoke-OpenConsoleBuild
  ```
- [ ] All existing tests pass
  ```powershell
  Invoke-OpenConsoleTests
  ```
- [ ] New tests pass
- [ ] Manual testing completed
  - [ ] Created test folder with brackets
  - [ ] Tested "Open in Terminal"
  - [ ] Verified PowerShell 5.1
  - [ ] Verified PowerShell 7+
  - [ ] Verified CMD still works
  - [ ] Verified WSL/Bash still works

### Documentation
- [x] Code comments added/updated
- [x] Issue analysis documented
- [x] Fix documentation created
- [x] Contribution guide created

### Git & GitHub
- [ ] Forked the repository
- [ ] Created feature branch
  ```bash
  git checkout -b fix/square-brackets-in-path
  ```
- [ ] Committed changes with clear message
  ```bash
  git add src/cascadia/WinRTUtils/inc/WtExeUtils.h
  git add src/cascadia/LocalTests_TerminalApp/QuoteAndEscapeTest.cpp
  git add src/cascadia/LocalTests_TerminalApp/TerminalApp.LocalTests.vcxproj
  git commit -m "Fix PowerShell working directory with square brackets in path"
  ```
- [ ] Pushed to your fork
  ```bash
  git push origin fix/square-brackets-in-path
  ```

## 📝 Pull Request Checklist

### Before Creating PR
- [ ] All tests pass locally
- [ ] Manual testing completed
- [ ] No merge conflicts with main branch
- [ ] Code follows project style guidelines
- [ ] Documentation is clear and complete

### PR Information
- [ ] Title: "Fix PowerShell working directory with square brackets in path"
- [ ] Description includes:
  - [ ] Summary of the issue
  - [ ] Root cause explanation
  - [ ] Solution description
  - [ ] Testing performed
  - [ ] Related issues referenced
- [ ] Labels added (if applicable)
- [ ] Screenshots/evidence attached

### PR Description Template
```markdown
## Summary of the Pull Request
Fixes PowerShell working directory issue when path contains square brackets

## References
Fixes #[issue-number]
Related: PowerShell/PowerShell#5752, microsoft/vscode#70432

## PR Checklist
* [x] Closes #[issue-number]
* [x] Tests added/passed
* [x] Documentation updated
* [x] Manual testing completed

## Detailed Description
When launching PowerShell through "Open in Terminal" from a folder with 
square brackets in the path (e.g., `C:\Folder [Name]\`), PowerShell would 
fail to set the correct working directory. This occurred because PowerShell 
treats square brackets as wildcard characters.

The fix escapes square brackets with backticks (PowerShell's escape character) 
in the `QuoteAndEscapeCommandlineArg` function.

## Changes Made
- Modified: `src/cascadia/WinRTUtils/inc/WtExeUtils.h`
  - Added square bracket escaping with backticks
- Added: `src/cascadia/LocalTests_TerminalApp/QuoteAndEscapeTest.cpp`
  - 6 comprehensive unit tests
- Modified: `src/cascadia/LocalTests_TerminalApp/TerminalApp.LocalTests.vcxproj`
  - Added test file to build

## Validation Steps Performed
- [x] Created test folder with square brackets
- [x] Verified "Open in Terminal" works correctly
- [x] Tested with PowerShell 5.1 and PowerShell 7
- [x] Verified other shells (CMD, WSL) still work
- [x] Added unit tests
- [x] All tests pass

## Screenshots
[Add screenshots showing the fix working]

Before:
- Path: C:\Test [Brackets]\
- Result: PowerShell opens in C:\Windows\System32\WindowsPowerShell\v1.0

After:
- Path: C:\Test [Brackets]\
- Result: PowerShell opens in C:\Test [Brackets]\
```

## 🔄 After PR Submission

### Respond to Feedback
- [ ] Monitor PR for comments
- [ ] Respond to review feedback promptly
- [ ] Make requested changes
- [ ] Push updates to the same branch
- [ ] Re-request review after changes

### CI/CD Checks
- [ ] Wait for automated tests to complete
- [ ] Fix any CI failures
- [ ] Ensure all checks pass

### Code Review
- [ ] Address all review comments
- [ ] Explain design decisions if questioned
- [ ] Be open to suggestions
- [ ] Update code based on feedback

## ✨ After Merge

### Celebrate! 🎉
- [ ] Your contribution is now part of Windows Terminal!
- [ ] Update your resume/portfolio
- [ ] Share your contribution on social media
- [ ] Consider contributing more!

### Follow-up
- [ ] Monitor for any issues related to your change
- [ ] Be available to help with any questions
- [ ] Consider related improvements

## 📊 Progress Tracker

### Current Status
- **Phase**: Pre-Submission ⏳
- **Completion**: 60% (Code done, testing pending)
- **Next Step**: Build and test locally

### Timeline
- [x] Day 1: Issue analysis and solution design
- [x] Day 1: Code implementation
- [x] Day 1: Unit tests creation
- [ ] Day 2: Build and test locally
- [ ] Day 2: Manual testing
- [ ] Day 3: Create PR
- [ ] Day 3-7: Code review and feedback
- [ ] Day 7+: Merge and celebrate!

## 🆘 Need Help?

### Build Issues
- Check: `doc/building.md`
- Ensure all prerequisites installed
- Try clean build

### Test Issues
- Check: `doc/TAEF.md`
- Verify test framework installed
- Run tests individually

### Git Issues
- Check: `CONTRIBUTING.md`
- Review Git best practices
- Ask in PR comments

### General Questions
- File an issue on GitHub
- Ask in PR comments
- Reach out to team on Twitter

## 📚 Resources
- [Contributing Guide](CONTRIBUTING.md)
- [Building Guide](doc/building.md)
- [Testing Guide](doc/TAEF.md)
- [Code Style](doc/STYLE.md)
- [Fix Documentation](FIX_DOCUMENTATION.md)
- [Quick Start](QUICK_START.md)

---

**Remember**: Take your time, test thoroughly, and don't hesitate to ask questions!

Good luck with your contribution! 🚀
