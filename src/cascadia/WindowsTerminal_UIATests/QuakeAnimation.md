# Checking the quake slide on a Windows desktop

Use an interactive session with animation effects enabled. In a test profile, set a distinctive solid background color so that a blank animation is easy to spot. Bind an unused global hotkey to `{"action":"globalSummon","name":"_quake","dropdownDuration":650}`. Start the window once before testing: creating `_quake` for the first time is not a restore from minimized.

- **Slide and frame:** Toggle twice per dock edge. In a slow recording, both dismiss and restore should have frames with *part* of the colored terminal visible. The text should travel with the edge; there should be no stationary full-size frame or Windows restore zoom. Checking only the final frame will miss a broken thumbnail.
- **Terminal size:** Run the following in PowerShell inside the test window while toggling. It prints the initial console dimensions and any changes. On the same monitor, dismiss/restore should produce no additional lines. On a mixed-DPI setup also try a stationary window straddling two offset displays; distinguish this from *requesting* a move to another monitor, which can legitimately resize the terminal once.

  ```powershell
  $previous = ''
  while ($true) {
      $size = '{0}x{1} / {2}x{3}' -f [Console]::WindowWidth, [Console]::WindowHeight, [Console]::BufferWidth, [Console]::BufferHeight
      if ($size -ne $previous) { $size; $previous = $size }
      Start-Sleep -Milliseconds 10
  }
  ```

- **Interruption and next restore:** Open the new-tab menu before dismissing; it should not stay behind or reappear on restore. Try toggling again mid-slide. With auto-hide enabled, switch focus during dismiss, then restore normally (from the taskbar if not minimizing to the notification area). The window should be visible at its original placement, not at an intermediate frame.
- **Duration:** Try `0` (no slide), `200` (the default for `quakeMode`), and `12000` (the slide should take about one second, not twelve). Disabling Windows animation effects should skip the slide.
