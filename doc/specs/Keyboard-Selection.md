---
author: Carlos Zamora @cazamor
created on: 2019-08-30
last updated: 2019-09-21
issue id: #715
---

# Keyboard Selection

## Abstract

This spec describes a new set of keybindings that allows the user to create and update a selection without the use of a mouse or stylus.

## Inspiration

ConHost allows the user to edit an existing selection using the keyboard. Holding `Shift` allows the user to move the second selection anchor in accordance with the arrow keys. The selection anchor updates by one cell per key event, allowing the user to refine the selected region.

Mark mode allows the users to update the selection anchors sequentially, and create a block selection.


## Solution Design

The fundamental solution design for keyboard selection is that the responsibilities between the Terminal Control and Terminal Core must be very distinct. The Terminal Control is responsible for handling user interaction and directing the Terminal Core to update the selection. The Terminal Core will need to update the selection according to the preferences of the Terminal Control.

### Fundamental Terminal Control Changes

`TermControl::_KeyDownHandler()` is responsible for interpretting the key events. At the time of writing this spec, there are 3 cases handled in this order:
- Alt+Gr events
- Keybindings
- Send Key Event (also clears selection)

Conditionally effective keybindings can be introduced to update the selection accordingly. A well known conditionally effective keybinding is the `Copy` action where text is copied if a selection is active, otherwise the key event is marked as `handled = false`. See "UI/UX Design" --> "Keybindings" for more information regarding keybindings.

#### Idea: Make keyboard selection a simulation of mouse selection
It may seem that some effort can be saved by making the keyboard selection act as a simulation of mouse selection. There is a union of mouse and keyboard activity that can be represented in a single set of selection motion interfaces that are commanded by the TermControl's Mouse/Keyboard handler and adapted into appropriate motions in the Terminal Core.

However, the mouse handler operates by translating a pixel coordinate on the screen to a text buffer coordinate. This would have to be rewritten and the approach was deemed unworthy.

### Fundamental Terminal Core Changes

The Terminal Core will need to expose a `KeyboardSelection()` function that is called by the keybinding handler. The following parameters will need to be passed in:
- `enum Direction`: the direction that the selection anchor will attempt to move to. Possible values include `Up`, `Down`, `Left`, and `Right`.
- `enum SelectionExpansionMode`: the selection expansion mode that the selection anchor will adhere to. Possible values include `Cell`, `Word`, and `Viewport`.

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
Mark Mode is a mode where the user can create more complete and complex selections using only the keyboard. It can be cycled int an enabled/disabled state through a user keybinding (`toggleMarkMode`).

Upon being enabled, the current cursor position becomes the selection anchor. The user can move the selection anchor using the same keybindings defined for keyboard selection (`moveSelectionAnchor` action).

#### `toggleMarkMode`
`toggleMarkMode` has two keybinding arguments:
| Parameter | Accepted Values | Default Value | Description |
|--|--|--|--|
| `anchorKeybinding` | regular keybindings and plain modifier keys | `Shift` | The keybinding used to anchor the selection anchor to it's current text buffer position. While holding down that keybinding, any `moveSelectionAnchor` actions will move the other selection anchor (`endSelectionPosition`). |
| `anchorMode` | `Hold`, `Toggle` | `Hold` | If `Hold`, the user needs to hold the `anchorKeybinding` keybinding to move the second selection anchor. If `Toggle`, the user switches between selection anchors when they press it. |

#### Rendering during Mark Mode
Since we are just moving the selection anchors, rendering the selection rects should operate normally. We need to ensure that we still scroll when we move a selection anchor past the top of bottom of the viewport.

In ConHost, output would be paused when a selection was present. This is a completely separate issue that is being tracked in (#2529)[https://github.com/microsoft/terminal/pull/2529].

#### Exiting Mark Mode
There are multiple ways to exit mark mode. The user can press...
- the `toggleMarkMode` keybinding
- the `ESC` key
- the `copy` keybinding (which also copies the contents of the selection to the user's clipboard)



<!--Holding down the `holdSelectionAnchor` keybinding action (set to `Shift` by default) anchors the selection anchor to it's current text buffer position. While holding down that keybinding, any `moveSelectionAnchor` actions will move the other selection anchor (`endSelectionPosition`).

**NOTE**: `holdSelectionAnchor` will have an `Enum anchorSwitching` parameter. Where `Hold` (default) is the functionality mentioned above, and `Toggle` switches between the selection anchors when pressed.-->


## UI/UX Design

### Keybindings

Thanks to Keybinding Args, there will only be 4 new commands that need to be added:
| Action | Keybinding Args | Description |
|--|--|--|
| `moveSelectionAnchor` |                                                               | If a selection exists, moves the last selection anchor.
|                       | `Enum direction { up, down, left, right}`                     | The direction the selection will be moved in. |
|                       | `Enum expansionMode { cell, word, line, viewport, buffer }`   | The context for which to move the selection anchor to. (defaults to `cell`)
| `selectEntireBuffer`  | | Select the entire text buffer.
| `toggleMarkMode`      | | Enter or exit mark mode. This allows you to create an entire selection using only the keyboard. |
| `holdSelectionAnchor` | | Enter or exit mark mode. This allows you to create an entire selection using only the keyboard. |

By default, the following keybindings will be set:
```JS
// Cell Selection
{ "command": { "action": "moveSelectionAnchor", "direction": "down" },  "keys": ["shift+down"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "up" },    "keys": ["shift+up"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "left" },  "keys": ["shift+left"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "right" }, "keys": ["shift+right"] },

// Word Selection
{ "command": { "action": "moveSelectionAnchor", "direction": "left",    "expansionMode": "word" }, "keys": ["ctrl+shift+left"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "right",   "expansionMode": "word" }, "keys": ["ctrl+shift+right"] },

// Viewport Selection
{ "command": { "action": "moveSelectionAnchor", "direction": "left",    "expansionMode": "viewport" }, "keys": ["shift+home"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "right",   "expansionMode": "viewport" }, "keys": ["shift+end"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "up",      "expansionMode": "viewport" }, "keys": ["ctrl+shift+up"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "down",    "expansionMode": "viewport" }, "keys": ["ctrl+shift+down"] },

// Buffer Corner Selection
{ "command": { "action": "moveSelectionAnchor", "direction": "up",      "expansionMode": "buffer" }, "keys": ["ctrl+shift+home"] },
{ "command": { "action": "moveSelectionAnchor", "direction": "down",    "expansionMode": "buffer" }, "keys": ["ctrl+shift+end"] },

// Select All
{ "command": "selectEntireBuffer", "keys": ["ctrl+shift+a"] },

// Mark Mode
{ "command": "toggleMarkMode", "keys": ["ctrl+shift+m"] },
```

## Capabilities

### Accessibility

Being able to create and update a selection without the use of a mouse is a very important feature to users with disabilities. The UI Automation system (particularly with the use of the signaling model), should be aware that a new selection exists. Thus, the bounding rects should be updated and the associated text regions should be reread appropriately.

### Security

N/A

### Reliability

With regards to the Terminal Core, the newly introduced code should rely on already existing and tested code. Thus no crash-related bugs are expected.

With regards to Terminal Control and the settings model, crash-related bugs are not expected. However, ensuring that the selection is updated and cleared in general use-case scenarious must be ensured.

### Compatibility

N/A

### Performance, Power, and Efficiency

## Potential Issues

The settings model makes all of these features easy to disable, if the user wishes to do so.

## Future considerations

### Expanding Mark Mode functionality
This spec currently limits mark mode functionality to the following:
- only move by cell (no other expansion modes)
- no rebinding arrow keys allowed
In addition to the `enum direction` and `enum expansionMode` args introduced above, a `bool markMode` arg can be added. If set to `true`, the selection anchor will adjust to these commands when in mark mode. This will allow the selection anchor to be able to be adjusted by different expansion modes.


## Resources

- https://blogs.windows.com/windowsdeveloper/2014/10/07/console-improvements-in-the-windows-10-technical-preview/
