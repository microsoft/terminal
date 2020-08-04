---
author: Carlos Zamora @carlos-zamora
created on: 2019-08-30
last updated: 2020-06-26
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

#### Moving by Cell
For `SelectionExpansionMode = Cell`, the selection anchor will be updated according to the buffer's output pattern. For **horizontal movements**, the selection anchor will attempt to move left or right. If a viewport boundary is hit, the anchor will wrap appropriately (i.e.: hitting the left boundary moves it to the last cell of the line above it).

For **vertical movements**, the selection anchor will attempt to move up or down. If a **viewport boundary** is hit and there is a scroll buffer, the anchor will move and scroll accordingly by a line. If a **buffer boundary** is hit, the anchor will not move. In this case, however, the event will still be considered handled.

**NOTE**: An important thing to handle properly in all cases is wide glyphs. The user should not be allowed to select a portion of a wide glyph; it should be all or none of it. When calling `_ExpandWideGlyphSelection` functions, the result must be saved to the anchor.

#### Moving by Word
For `SelectionExpansionMode = Word`, the selection anchor will also be updated according to the buffer's output pattern, as above. However, the selection will be updated in accordance with "chunk selection" (performing a double-click and dragging the mouse to expand the selection). For **horizontal movements**, the selection anchor will be updated according to the `_ExpandDoubleClickSelection` functions. The result must be saved to the anchor. As before, if a boundary is hit, the anchor will wrap appropriately. See [Future Considerations](#FutureConsiderations) for how this will interact with line wrapping.

For **vertical movements**, the movement is a little more complicated than before. The selection will still respond to buffer and viewport boundaries as before. If the user is trying to move up, the selection anchor will attempt to move up by one line, then selection will be expanded leftwards. Alternatively, if the user is trying to move down, the selection anchor will attempt to move down by one line, then the selection will be expanded rightwards.

#### Moving by Viewport
For `SelectionExpansionMode = Viewport`, the selection anchor will be updated according to the viewport's height. Horizontal movements will be updated according to the viewport's width, thus resulting in the anchor being moved to the left/right boundary of the viewport.

#### Moving by Buffer

For `SelectionExpansionMode = Buffer`, the selection anchor will be moved to the beginning or end of all the text within the buffer. If moving up or left, set the position to 0,0 (the origin of the buffer). If moving down or right, set the position to the last character in the buffer.


**NOTE**: In all cases, horizontal movements attempting to move past the left/right viewport boundaries result in a wrap. Vertical movements attempting to move past the top/bottom viewport boundaries will scroll such that the selection is at the edge of the screen. Vertical movements attempting to move past the top/bottom buffer boundaries will be clamped to be within buffer boundaries.

Every combination of the `Direction` and `SelectionExpansionMode` will map to a keybinding. These pairings are shown below in the UI/UX Design --> Keybindings section.



### Copy Mode
Copy Mode is a mode where the user can create and modify a selection using only the keyboard. The following flowchart covers how the new `copyMode` keybinding works:

![Copy Mode Flowchart][images/CopyModeFlowchart.png]

If a selection is not active, a "start" and "end" selection point is created at the cursor position. `moveSelectionPoint` calls then move "start" and "end" together as one position. This position will be denoted with a "y-beam".

![Y-Beam Example][images/Y-Beam.png]

If a selection is active, `copyMode` leaves the selection untouched, all subsequent `moveSelectionPoint` calls move "start". The y-beam is then cut vertically in half, where one half is drawn on the "start" endpoint, and the other half is drawn on the "end" endpoint. Since the user is moving "start", only the half-y-beam is drawn on the "start" endpoint.

![Separated Y-Beam Example][images/Half-Y-Beam.png]

**NOTE:** Both half y-beams could have been presented as shown in the image below. This idea was omitted because then there is no indication for which half y-beam is currently focused.

![Both Separated Y-Beams Example][images/Split-Y-Beam.png]

Pressing `copyMode` again, will then anchor "start". Subsequent `moveSelectionPoint` calls move the "end" selection point. As before, the half-y-beam is only drawn on the "end" to denote that this is the endpoint that is being moved.

Pressing `copyMode` essentially cycles between which selection point is targeted. The half-y-beam is drawn on the targeted endpoint.

#### Block Selection
A user can normally create a block selection by holding <kbd>alt</kbd> then creating a selection.

If the user is in Copy Mode, and desires to make a block selection, they can use the `toggleBlockSelection` keybinding. `toggleBlockSelection` takes an existing selection, and transforms it into a block selection.

All selections created in Copy Mode will have block selection disabled by default.

#### Rendering during Copy Mode
Since we are just moving the selection points, rendering the selection rects should operate normally. We need to ensure that we still scroll when we move a selection point past the top/bottom of the viewport.

In ConHost, output would be paused when a selection was present. Windows Terminal does not pause the output when a selection is present, however, it does not scroll to the new output.

#### Interaction with CopyOnSelect
If `copyOnSelect` is enabled, the selection is copied when the selection operation is "complete". Thus, the selection is copied when the `copy` keybinding is used or the selection is copied using the mouse.

#### Interaction with Mouse Selection
If a selection exists, the user is basically already in Copy Mode. The user should be modifying the "end" endpoint of the selection when using the `moveSelectionPoint` keybindings. The existing selection should not be cleared (contrary to prior behavior). However, the half-y-beam will not be drawn. Once the user presses the `copyMode` or `moveSelectionPoint` keybinding, the half-y-beam is drawn on the targeted endpoint (which will then be "start").

During Copy Mode, if the user attempts to create a selection using the mouse, any existing selections are cleared and the mouse creates a selection normally. However, contrary to prior behavior, the user will still be in Copy Mode. The target endpoint being modified in Copy Mode, however, will be the "end" endpoint of the selection, instead of the cursor (as explained earlier in the flowchart).


#### Exiting Copy Mode
The user exits copy mode when the selection is cleared. Thus, the user can press...
- the `ESC` key
- the `copy` keybinding (which also copies the contents of the selection to the user's clipboard)
- keys that generate input and clear a selection

In all of these cases, the selection will be cleared.

If `copyOnSelect` is enabled, `ESC` is interpreted as "cancelling" the selection, so nothing is copied. Keys that generate input are also interpreted as "cancelling" the selection. Only the `copy` keybinding or copying using the mouse is considered "completing" the selection operation, and copying the content to the clipboard.

NOTE - Related to #3884:
    If the user has chosen to have selections persist after a copy operation, the selection created by Copy Mode is treated no differently than one created with the mouse. The selection will persist after a copy operation. However, if the user exits Copy Mode in any of the other situations, the selection is cleared.





## UI/UX Design

### Keybindings

Thanks to Keybinding Args, there will only be 2 new commands that need to be added:
| Action | Keybinding Args | Description |
|--|--|--|
| `moveSelectionPoint` |                                                               | If a selection exists, moves the last selection anchor.
|                       | `Enum direction { up, down, left, right }`                     | The direction the selection will be moved in. |
|                       | `Enum expandBy { cell, word, page, all }`   | The context for which to move the selection anchor to. (defaults to `cell`)
| `selectAll`  | | Select the entire text buffer.
| `copyMode`      | | Cycle the selection point targeted by `moveSelectionPoint`. If no selection exists, a selection is created at the cursor. |
| `toggleBlockSelection`      | | Transform the existing selection between a block selection and a line selection.  |


By default, the following keybindings will be set:
```JS
// Cell Selection
{ "command": { "action": "moveSelectionPoint", "direction": "down" },  "keys": "shift+down" },
{ "command": { "action": "moveSelectionPoint", "direction": "up" },    "keys": "shift+up" },
{ "command": { "action": "moveSelectionPoint", "direction": "left" },  "keys": "shift+left" },
{ "command": { "action": "moveSelectionPoint", "direction": "right" }, "keys": "shift+right" },

// Word Selection
{ "command": { "action": "moveSelectionPoint", "direction": "left",    "expandBy": "word" }, "keys": "ctrl+shift+left" },
{ "command": { "action": "moveSelectionPoint", "direction": "right",   "expandBy": "word" }, "keys": "ctrl+shift+right" },

// Viewport Selection
{ "command": { "action": "moveSelectionPoint", "direction": "left",    "expandBy": "page" }, "keys": "shift+home" },
{ "command": { "action": "moveSelectionPoint", "direction": "right",   "expandBy": "page" }, "keys": "shift+end" },
{ "command": { "action": "moveSelectionPoint", "direction": "up",      "expandBy": "page" }, "keys": "shift+pgup" },
{ "command": { "action": "moveSelectionPoint", "direction": "down",    "expandBy": "page" }, "keys": "shift+pgdn" },

// Buffer Corner Selection
{ "command": { "action": "moveSelectionPoint", "direction": "up",      "expandBy": "all" }, "keys": "ctrl+shift+home" },
{ "command": { "action": "moveSelectionPoint", "direction": "down",    "expandBy": "all" }, "keys": "ctrl+shift+end" },

// Select All
{ "command": "selectAll", "keys": "ctrl+shift+a" },

// Copy Mode
{ "command": "copyMode", "keys": "ctrl+shift+m" },
{ "command": "toggleBlockSelection", "keys": "alt+shift+m" },
```
These are in accordance with ConHost's keyboard selection model.

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

### Grapheme Clusters
When grapheme cluster support is inevitably added to the Text Buffer, moving by "cell" is expected to move by "character" or "cluster". This is similar to how wide glyphs are handled today. Either all of it is selected, or none of it.

### Word Selection Wrap
At the time of writing this spec, expanding or moving by word is interrupted by the beginning or end of the line, regardless of the wrap flag being set. In the future, selection and the accessibility models will respect the wrap flag on the text buffer.

### Contextual Keybindings
This feature introduces a large number of keybindings that only work if a selection is active. Currently, key bindings cannot be bound to a context, so if a user binds `moveSelectionPoint` to `shift+up` and there is no selection, `shift+up` is sent directly to the Terminal. In the future, a `context` key could be added to new bindings to get around this problem. That way, users could bind other actions to `shift+up` to run specifically when a selection is not active.

### Copy Mode - Start in Block Selection
If requested by a user, a boolean `blockMode` keybinding argument can be added to `copyMode`. It will default to false. A user can set it to true to have `copyMode` start in block selection mode.


## Resources

- https://blogs.windows.com/windowsdeveloper/2014/10/07/console-improvements-in-the-windows-10-technical-preview/
