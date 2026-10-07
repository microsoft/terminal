# Pinned tabs

Right-click a tab and choose **Pin tab**. Pinned tabs stay on the left, show
their icons instead of their titles, and have no close button. Hover over a tab
to read its title. Choose **Unpin tab** to return it to normal size.

Drag moves and keyboard moves keep pinned tabs together. New tabs appear after
the pinned tabs, including when `newTabPosition` is `afterCurrentTab`.

**Close other tabs** and **Close tabs after** skip pinned tabs. Closing a tab
through its menu or a keyboard shortcut still works. Middle-click leaves pinned
tabs open. Closing the window and
the existing behaviour when a shell exits also apply to pinned tabs.

Pins are saved with the existing window layout. Enable **Settings > Startup >
When Terminal starts > Open windows from a previous session** to restore them
on startup. Moving a tab to another window keeps its pin.

The command palette includes **Pin or unpin tab**. To assign a keyboard shortcut,
add a key binding for `Terminal.ToggleTabPinned`, for example:

```json
{ "keys": "ctrl+alt+shift+p", "id": "Terminal.ToggleTabPinned" }
```

Custom actions can use `toggleTabPinned`.

## Checks on Windows

Build the application and run the `TabTests` tests in `LocalTests_TerminalApp`.
The pin tests cover ordering, new tab placement, bulk closing and saved actions.

Check the app with each tab width setting and with a screen reader. Pin and
unpin a background tab, rename a pinned tab, and move tabs across the pinned
group by mouse and keyboard. Check that titles remain available in tooltips
and that progress and bell indicators remain visible.

Enable session restore, close the window, and reopen it. Check the pinned tabs
and their order. Also check a tab moved to another window and a pinned Settings
tab.
