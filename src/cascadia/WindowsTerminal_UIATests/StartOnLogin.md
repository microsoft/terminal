# Startup task regression checks

Use the packaged Dev build in a disposable Windows user account. Launching
`wtd.exe` from a Startup folder shortcut is **not** a startup task activation.
Disable Windows' “restartable apps” sign-in option so that it doesn't relaunch
the app independently of the startup task.

1. In Terminal Settings > Startup, enable **Start on user login**, choose
   **Minimized**, and save. Leave the page open. Disable Terminal Dev in
   Windows Settings > Apps > Startup, then return to Terminal: the checkbox
   must be unchecked and the picker disabled, still displaying Minimized.
   Re-enable it externally and return: the checkbox and picker should be
   enabled again without navigating away. Changing the mode in JSON must
   never enable a disabled startup task.
2. Set `firstWindowPreference` to `persistedLayoutAndContent`. Open two
   windows with different text in each, maximize one, and sign out with both
   open. On sign-in both must be minimized, not in the foreground. Restore
   each from the taskbar; verify its tabs and text, and that restoring the
   maximized window returns it to its maximized size.
3. Repeat with **Headless** and `compatibility.allowHeadless: false`.
   Verify the process is running but there is no Terminal window. Wait at
   least six minutes (one persistence tick), then sign out and back in
   without opening a window. A plain `wtd` should now restore both windows
   with their text, with no extra default window. Note the PID, close both
   windows, and verify that the same process remains with no windows. Open
   `wtd` again: the same PID should own one new window, not the closed layouts.
   Change the login mode to **Normal** and close the window; the same process
   should still be running. As a control, exit Terminal completely and start it
   manually with **Headless** saved: closing the last window should exit when
   `compatibility.allowHeadless` is false.
4. For warm activation, have an independent sign-in task open a normal
   Terminal Dev window *before* Windows runs the Terminal startup task. Set
   `windowingBehavior` to `useAnyExisting`. Headless must leave that window
   alone and keep its process running after its last window closes;
   Minimized must create a separate minimized window without moving
   focus, adding a tab to the existing window, or minimizing it. With the
   default **Normal** mode, startup should behave like an ordinary launch.
