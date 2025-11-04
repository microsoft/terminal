## Summary of the Pull Request

This PR fixes three GitHub issues:
- **#19493**: `suppressApplicationTitle` setting doesn't work properly
- **#19488**: Terminal scrolls back down when pressing ctrl-shift-M with no marks
- **#13388**: Cannot type in new pane or tab until focus is manually activated with mouse click

## References and Relevant Issues

- Closes #19493
- Closes #19488
- Closes #13388

## Detailed Description of the Pull Request / Additional comments

### Fix #19493: `suppressApplicationTitle` doesn't work
**Problem**: The `suppressApplicationTitle` setting was not being consistently applied when its value was `false`. The code had a conditional check `if (profile.SuppressApplicationTitle())` which meant the setting was only propagated if it was explicitly `true`.

**Solution**: Modified `TerminalSettings.cpp` to always assign the `_SuppressApplicationTitle` value from the profile, ensuring `false` is also correctly applied.

**File Changed**: `src/cascadia/TerminalSettingsAppAdapterLib/TerminalSettings.cpp`

### Fix #19488: Don't scroll back down on ctrl-shift-M
**Problem**: When using `ScrollToMark` with `Next` or `Last` directions, if no mark was found, the terminal would scroll to the very bottom of the buffer, which was an unexpected behavior.

**Solution**: Modified `ControlCore.cpp` to prevent scrolling to the bottom when no mark is found for the `Next` direction. For `Last` direction, it still scrolls to the bottom if no mark is found, which is the expected behavior.

**File Changed**: `src/cascadia/TerminalControl/ControlCore.cpp`

### Fix #13388: Cannot type in new pane or tab until focus is manually activated
**Problem**: When a new pane or tab was created, the terminal control within it was not receiving immediate keyboard focus, requiring a manual mouse click. While `_UpdateActivePane()` marked the pane as active, it didn't explicitly focus the content.

**Solution**: Modified `Tab.cpp` in the `_UpdateActivePane` method to explicitly call `content.Focus(FocusState::Programmatic)` on the pane's content. This ensures that the terminal control receives programmatic focus immediately upon activation.

**File Changed**: `src/cascadia/TerminalApp/Tab.cpp`

## Validation Steps Performed

### For #19493 (`suppressApplicationTitle`):
1. Set `suppressApplicationTitle` to `false` in a profile
2. Created a new tab with that profile
3. Verified that the application title is updated by the terminal (not suppressed)
4. Set `suppressApplicationTitle` to `true` in a profile
5. Created a new tab with that profile
6. Verified that the application title is suppressed (not updated by terminal)

### For #19488 (Scroll to mark):
1. Opened a terminal with no marks
2. Pressed `ctrl-shift-M` (Scroll to Next Mark)
3. Verified that the terminal does NOT scroll to the bottom (expected: no action)
4. Created some marks in the terminal
5. Pressed `ctrl-shift-M` multiple times
6. Verified that it correctly scrolls between marks
7. Tested `ctrl-shift-N` (Scroll to Last Mark) - verified it still scrolls to bottom when no marks exist

### For #13388 (Focus on new pane/tab):
1. Created a new pane using split pane command
2. Verified that keyboard input works immediately without clicking
3. Created a new tab
4. Verified that keyboard input works immediately without clicking
5. Switched between panes using keyboard shortcuts
6. Verified focus is correctly set on the newly active pane

## PR Checklist
- [x] Closes #19493
- [x] Closes #19488
- [x] Closes #13388
- [ ] Tests added/passed (Note: Manual testing performed, but should verify if automated tests exist for these scenarios)
- [ ] Documentation updated
   - If checked, please file a pull request on [our docs repo](https://github.com/MicrosoftDocs/terminal) and link it here: #xxx
- [ ] Schema updated (if necessary)
