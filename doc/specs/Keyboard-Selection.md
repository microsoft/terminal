---
author: Carlos Zamora @carlos-zamora
created on: 2019-08-30
last updated: 2020-06-23
issue id: 715
---

# Keyboard Selection

## Abstract

This spec describes a new set of keybindings that allows the user to create and update a selection without the use of a mouse or stylus.

## Inspiration

ConHost allows the user to modify a selection using the keyboard. Holding `Shift` allows the user to move the second selection endpoint in accordance with the arrow keys. The selection anchor updates by one cell per key event, allowing the user to refine the selected region.

### Creating a selection
Mark Mode is a ConHost feature that allows the user to create a selection using only the keyboard. In CMD, pressing <kbd>ctrl+m</kbd> toggles mark mode. The current cursor position becomes a selection endpoint. The user can use the arrow keys to move that endpoint. While the user then holds <kbd>shift</kbd>, the selection endpoint ('start') is anchored to it's current position, and the arrow keys move the other selection endpoint ('end'). After a selection is made, the user can copy the selected text.

Other terminal emulators have different approaches to this feature. iTerm2, for example, has Copy Mode (documentation [linked here](https://iterm2.com/documentation-copymode.html)). Here, <kbd>cmd+shift+c</kbd> makes the current cursor position become a selection endpoint. The arrow keys can be used to move that endpoint. However, unlike Mark Mode, a keybinding <kbd>c+space</kbd> is used to change the start/stop selecting. The first time it's pressed, the 'start' endpoint is anchored. The second time it's pressed, the 'end' endpoint is set. After this, you can still move a cursor, but the selection persists until a new selection is created (either by pressing the keybinding again, or using the mouse).

Though tmux is not a terminal emulator, it does also have Copy Mode that behaves fairly similarly to that of iTerm2's.


## Solution Design

The fundamental solution design for keyboard selection is that the responsibilities between the Terminal Control and Terminal Core must be very distinct. The Terminal Control is responsible for handling user interaction and directing the Terminal Core to update the selection. The Terminal Core will need to update the selection according to the preferences of the Terminal Control.


### Fundamental Terminal Control Changes

`TermControl::_KeyDownHandler()` is responsible for interpreting the key events. At the time of writing this spec, there are 3 cases handled in this order:
- Alt+Gr events
- Keybindings
- Send Key Event (also clears selection)

Conditionally effective keybindings can be introduced to update the selection accordingly. A well known conditionally effective keybinding is the `Copy` action where text is copied if a selection is active, otherwise the key event is marked as `handled = false`. See "UI/UX Design" --> "Keybindings" for more information regarding keybindings.

#### Idea: Make keyboard selection a simulation of mouse selection
It may seem that some effort can be saved by making the keyboard selection act as a simulation of mouse selection. There is a union of mouse and keyboard activity that can be represented in a single set of selection motion interfaces that are commanded by the TermControl's Mouse/Keyboard handler and adapted into appropriate motions in the Terminal Core.

However, the mouse handler operates by translating a pixel coordinate on the screen to a text buffer coordinate. This would have to be rewritten and the approach was deemed unworthy.


### Fundamental Terminal Core Changes

The Terminal Core will need to expose a `MoveSelectionEndpoint()` function that is called by the keybinding handler. The following parameters will need to be passed in:
- `enum Direction`: the direction that the selection anchor will attempt to move to. Possible values include `Up`, `Down`, `Left`, and `Right`.
- `enum SelectionExpansionMode`: the selection expansion mode that the selection anchor will adhere to. Possible values include `Cell`, `Word`, `Viewport`, `Buffer`.

For `SelectionExpansionMode = Cell`, the selection anchor will be updated according to the buffer's output pattern. For **horizontal movements**, the selection anchor will attempt to move left or right. If a viewport boundary is hit, the anchor will wrap appropriately (i.e.: hitting the left boundary moves it to the last cell of the line above it).

For **vertical movements**, the selection anchor will attempt to move up or down. If a **viewport boundary** is hit and there is a scroll buffer, the anchor will move and scroll accordingly by a line. If a **buffer boundary** is hit, the anchor will not move. In this case, however, the event will still be considered handled.

For `SelectionExpansionMode = Word`, the selection anchor will also be updated according to the buffer's output pattern, as above. However, the selection will be updated in accordance with "chunk selection" (performing a double-click and dragging the mouse to expand the selection). For **horizontal movements**, the selection anchor will be updated according to the `_ExpandDoubleClickSelection` functions. The result must be saved to the anchor. As before, if a boundary is hit, the anchor will wrap appropriately.

For **vertical movements**, the movement is a little more complicated than before. The selection will still respond to buffer and viewport boundaries as before. If the user is trying to move up, the selection anchor will attempt to move up by one line, then selection will be expanded leftwards. Alternatively, if the user is trying to move down, the selection anchor will attempt to move down by one line, then the selection will be expanded rightwards.

For `SelectionExpansionMode = Viewport`, the selection anchor will be updated according to the viewport's height. Horizontal movements will be updated according to the viewport's width, thus resulting in the anchor being moved to the left/right boundary of the viewport.

**NOTE**: An important thing to handle properly in all cases is wide glyphs. The user should not be allowed to select a portion of a wide glyph; it should be all or none of it. When calling `_ExpandWideGlyphSelection` functions, the result must be saved to the anchor.

**NOTE**: In all cases, horizontal movements attempting to move past the left/right viewport boundaries result in a wrap. Vertical movements attempting to move past the top/bottom viewport boundaries will scroll such that the selection is at the edge of the screen. Vertical movements attempting to move past the top/bottom buffer boundaries will be clamped to be within buffer boundaries.

Every combination of the `Direction` and `SelectionExpansionMode` will map to a keybinding. These pairings are shown below in the UI/UX Design --> Keybindings section.

**NOTE**: If `copyOnSelect` is enabled, we need to make sure we update the clipboard on every change in selection.



### Mark Mode
Mark Mode is a mode where the user can begin a selection using only the keyboard. It can be cycled into an enabled/disabled state through a user keybinding (`toggleMarkMode`).

Upon being enabled, the current cursor position becomes the selection endpoint. The user can move the selection anchor using the same keybindings defined for keyboard selection (`moveSelectionPoint` action).

#### `toggleMarkMode`
`toggleMarkMode` has two keybinding arguments:
| Parameter | Accepted Values | Default Value | Description |
|--|--|--|--|
| `anchorModifier` | regular keybindings and plain modifier keys | `Shift` | The keybinding used to anchor the selection endpoint to its current text buffer position. While holding down that keybinding, any `moveSelectionPoint` actions will move the other selection anchor (`endSelectionPosition`). |
| `anchorMode` | `Hold`, `Toggle` | `Hold` | If `Hold`, the user needs to hold the `anchorModifier` key to anchor the 'start' selection endpoint and move the second selection endpoint (like Mark Mode). If `Toggle`, the user switches between selection endpoints when they press it (like Copy Mode). |

#### Rendering during Mark Mode
Since we are just moving the selection anchors, rendering the selection rects should operate normally. We need to ensure that we still scroll when we move a selection anchor past the top/bottom of the viewport.

In ConHost, output would be paused when a selection was present. This is a completely separate issue that is being tracked in (#2529)[https://github.com/microsoft/terminal/pull/2529].

#### Interaction with CopyOnSelect
If `copyOnSelect` is enabled, the selection is copied when the selection operation is "complete". If `anchorMode=Hold`, the user has to use the `copy` keybinding to signify that they have finished creating a selection. If `anchorMode=Toggle`, the selection is copied either when the `copy` keybinding is used, or when the user presses the `anchorModifier` key and the 'end' endpoint is set.

#### Mouse Interaction and Mark Mode
If a selection exists prior to entering Mark Mode, upon entering Mark Mode, the user should be modifying the "end" endpoint of the selection, instead of the cursor. The existing selection should not be cleared (contrary to prior behavior).

During Mark Mode, if the user attempts to create a selection using the mouse, any existing selections are cleared and the mouse creates a selection normally. However, contrary to prior behavior, the user will still be in Mark Mode. The target endpoint being modified in Mark Mode, however, will be the "end" endpoint of the selection, instead of the cursor (as explained earlier).


#### Exiting Mark Mode
There are multiple ways to exit mark mode. The user can press...
- the `toggleMarkMode` keybinding
- the `ESC` key
- the `copy` keybinding (which also copies the contents of the selection to the user's clipboard)

In all three cases, the selection will be cleared.

NOTE - Related to #3884:
    If the user has chosen to have selections persist after a copy operation, the selection created by Mark Mode is treated no differently than one created with the mouse. The selection will persist after a copy operation.





## UI/UX Design

### Keybindings

Thanks to Keybinding Args, there will only be 2 new commands that need to be added:
| Action | Keybinding Args | Description |
|--|--|--|
| `moveSelectionPoint` |                                                               | If a selection exists, moves the last selection anchor.
|                       | `Enum direction { up, down, left, right}`                     | The direction the selection will be moved in. |
|                       | `Enum expansionMode { cell, word, viewport, buffer }`   | The context for which to move the selection anchor to. (defaults to `cell`)
| `selectEntireBuffer`  | | Select the entire text buffer.
| `toggleMarkMode`      | | Enter or exit mark mode. This allows you to create an entire selection using only the keyboard. |


By default, the following keybindings will be set:
```JS
// Cell Selection
{ "command": { "action": "moveSelectionPoint", "direction": "down" },  "keys": "shift+down" },
{ "command": { "action": "moveSelectionPoint", "direction": "up" },    "keys": "shift+up" },
{ "command": { "action": "moveSelectionPoint", "direction": "left" },  "keys": "shift+left" },
{ "command": { "action": "moveSelectionPoint", "direction": "right" }, "keys": "shift+right" },

// Word Selection
{ "command": { "action": "moveSelectionPoint", "direction": "left",    "expansionMode": "word" }, "keys": "ctrl+shift+left" },
{ "command": { "action": "moveSelectionPoint", "direction": "right",   "expansionMode": "word" }, "keys": "ctrl+shift+right" },

// Viewport Selection
{ "command": { "action": "moveSelectionPoint", "direction": "left",    "expansionMode": "viewport" }, "keys": "shift+home" },
{ "command": { "action": "moveSelectionPoint", "direction": "right",   "expansionMode": "viewport" }, "keys": "shift+end" },
{ "command": { "action": "moveSelectionPoint", "direction": "up",      "expansionMode": "viewport" }, "keys": "shift+pgup" },
{ "command": { "action": "moveSelectionPoint", "direction": "down",    "expansionMode": "viewport" }, "keys": "shift+pgdn" },

// Buffer Corner Selection
{ "command": { "action": "moveSelectionPoint", "direction": "up",      "expansionMode": "buffer" }, "keys": "ctrl+shift+home" },
{ "command": { "action": "moveSelectionPoint", "direction": "down",    "expansionMode": "buffer" }, "keys": "ctrl+shift+end" },

// Select All
{ "command": "selectEntireBuffer", "keys": "ctrl+shift+a" },

// Mark Mode
{ "command": { "action": "toggleMarkMode", "anchorModifier": "shift", "anchorMode": "toggle" }, "keys": "ctrl+shift+m" },
```

## Capabilities

### Accessibility

Using the keyboard is generally a more accessible experience than using the mouse. Being able to modify a selection by using the keyboard is a good first step towards making selecting text more accessible.

Being able to create and update a selection without the use of a mouse is a very important feature to users with disabilities. The UI Automation system is already set up to detect changes to the active selection, so as long as the UiaRenderer is notified of changes to the selection, this experience should be accessible.

### Security

N/A

### Reliability

With regards to the Terminal Core, the newly introduced code should rely on already existing and tested code. Thus no crash-related bugs are expected.

With regards to Terminal Control and the settings model, crash-related bugs are not expected. However, ensuring that the selection is updated and cleared in general use-case scenarios must be ensured.

### Compatibility

N/A

### Performance, Power, and Efficiency

## Potential Issues

The settings model makes all of these features easy to disable, if the user wishes to do so.

## Future considerations

None


## Resources

- https://blogs.windows.com/windowsdeveloper/2014/10/07/console-improvements-in-the-windows-10-technical-preview/
