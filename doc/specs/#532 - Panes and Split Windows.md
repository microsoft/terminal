---
author: "Mike Griese @zadjii-msft"
created on: 2019-05-16
last updated: 2019-07-07
issue id: 523
---

# Panes in the Windows Terminal

## Abstract

Panes are an abstraction by which the terminal can display multiple terminal
instances simultaneously in a single terminal window. While tabs allow for a
single terminal window to have many terminal sessions running simultaneously
within a single window, only one tab can be visible at a time. Panes, on the
other hand, allow a user to have many different terminal sessions visible to the
user within the context of a single window at the same time. This can enable
greater productivity from the user, as they can see the output of one terminal
window while working in another.

This spec will help outline the design of the implementation of panes in the
Windows Terminal.

## Inspirations

Panes within the context of a single terminal window are not a new idea. The
design of the panes for the Windows Terminal was heavily inspired by the
application `tmux`, which is a commandline application which acts as a "terminal
multiplexer", allowing for the easy management of many terminal sessions from a
single application.

Other applications that include pane-like functionality include (but are not
limited to):

* screen
* terminator
* emacs & vim
* Iterm2

## Design

The architecture of the Windows Terminal can be broken into two main pieces:
Tabs and Panes. The Windows Terminal supports _top-level_ tabs, with nested
panes inside the tabs. This means that there's a single strip of tabs along the
application, and each tab has a set of panes that are visible within the context
of that tab.

Panes are implemented as a binary tree of panes. A Pane can either be a leaf
pane, with it's own terminal control that it displays, or it could be a parent
pane, where it has two children, each with their own terminal control.

When a pane is a parent, its two children are either split vertically or
horizontally. Parent nodes don't have a terminal of their own, they merely
display the terminals of their children.

 * If a Pane is split vertically, the two panes are seperated by a vertical
   split, as to appear side-by-side. Think `[|]`
 * If a Pane is split horizontally, the two panes are split by a horizontal
   separator, and appear above/below one another. Think `[-]`.

As additional panes are created, panes will continue to subdivide the space of
their parent. It's up to the parent pane to control the sizing and display of
it's children.

### Example

We'll start by taking the terminal and creating a single vertical split. There
are now two panes in the terminal, side by side. The original terminal is `A`,
and the newly created one is `B`. The terminal now looks like this:

```
  +---------------+
  |       |       |    1: parent [|]
  |       |       |    ├── 2: A
  |       |       |    └── 3: B
  |   A   |   B   |
  |       |       |
  |       |       |
  |       |       |
  +---------------+
```

Here, there are actually 3 nodes: 1 is the parent of both 2 and 3. 2 is the node
containing the `A` terminal, and 3 is the node with the `B` terminal.


We could now split `B` in two horizontally, creating a third terminal pane `C`.

```
  +---------------+
  |       |       |    1: parent [|]
  |       |   B   |    ├── 2: A
  |       |       |    └── 3: parent [-]
  |   A   +-------+        ├── 4: B
  |       |       |        └── 5: C
  |       |   C   |
  |       |       |
  +---------------+
```

Node 3 is now a parent node, and the terminal `B` has moved into a new node as a
sibling of the new terminal `C`.

We could also split `A` in horizontally, creating a fourth terminal pane `D`.

```
  +---------------+
  |       |       |    1: parent [|]
  |   A   |   B   |    ├── 2: parent [-]
  |       |       |    |   ├── 4: A
  +-------+-------+    |   └── 5: D
  |       |       |    └── 3: parent [-]
  |   D   |   C   |        ├── 4: B
  |       |       |        └── 5: C
  +---------------+
```

While it may appear that there's a single horizontal separator and a single
vertical separator here, that's not actually the case. Due to the tree-like
structure of the pane splitting, the horizontal splits exist only between the
two panes they're splitting. So, the user could move each of the horizontal
splits independently, without affecting the other set of panes. As an example:

```
  +---------------+
  |       |       |
  |   A   |       |
  +-------+   B   |
  |       |       |
  |   D   |       |
  |       +-------+
  |       |   C   |
  +---------------+
```

### Creating a pane

In the basic use case, the user will decide to split the currently focused pane.
The currently focused pane is always a leaf, because as parent's can't be
focused (they don't have their own terminal). When a user decides to add a new
pane, the child will:

 1. Convert into a parent
 2. Move its terminal into its first child
 3. Split its UI in half, and display each child in one half.

It's up to the app hosting the panes to tell the pane what kind of terminal in
wants created in the new pane. By default, the new pane will be created with the
default settings profile.

### While panes are open

When a tab has multiple panes open, only one is the "active" pane. This is the
pane that was last focused in the tab. If the tab is the currently open tab,
then this is the pane with the currently focused terminal control. When the user
brings the tab into focus, the last focused pane is the pane that should become
focused again.

The tab's state will be updated to reflect the state of it's focused pane. The
title text and icon of the tab will reflect that of the focused pane. Should the
focus switch from one pane to another, the tab's text and icon should update to
reflect the newly focused control. Any additional state that the tab would
display for a single pane should also be reflected in the tab for a tab with
multiple panes.

While panes are open, the user should be able to move any split between panes.
In moving the split, the sizes of the terminal controls should be resized to
match.

### Closing a pane

A pane can either be closed by the user manually, or when the terminal it's
attached to raises its ConnectionClosed event. When this happens, we should
remove this pane from the tree. The parent of the closing pane will have to
remove the pane as one of it's children. If the sibling of the closing pane is a
leaf, then the parent should just take all of the state from the remaining pane.
This will cause the remaining pane's content to expand to take the entire
boundaries of the parent's pane. If the remaining child was a parent itself,
then the parent will take both the children of the remaining pane, and make them
the parent's children, as if the parent node was taken from the tree and
replaced by the remaining child.

## Future considerations

The Pane implementation isn't complete in it's current form. There are many
additional things that could be done to improve the user experience. This is by
no means a comprehensive list.

* [ ] Panes should be resizable with the mouse. The user should be able to drag
  the separator for a pair of panes, and have the content between them resize as
  the separator moves.
* [ ] There's no keyboard shortcut for "ClosePane"
* [ ] The user should be able to configure what profile is used for splitting a
  pane. Currently, the default profile is used, but it's possible a user might
  want to create a new pane with the parent pane's profile.
* [ ] There should be some sort of UI to indicate that a particular pane is
  focused, more than just the blinking cursor. `tmux` accomplishes this by
  colorizing the separators adjacent to the active pane. Another idea is
  displaying a small outline around the focused pane (like when tabbing through
  controls on a webpage).
* [ ] The user should be able to navigate the focus of panes with the keyboard,
  instead of requiring the mouse.
* [ ] The user should be able to zoom a pane, to make the pane take the entire
  size of the terminal window temporarily.
* [ ] A pane doesn't necessarily need to host a terminal. It could potentially
  host another UIElement. One could imagine enabling a user to quickly open up a
  Browser pane to search for a particular string without needing to leave the
  terminal.

## Footnotes

### Why not top-level panes, and nested tabs?

If each pane were to have it's own set of tabs, then each pane would need to
reserve screen real estate for a row of tabs. As a user continued to split the
window, more and more of the screen would be dedicated to just displaying a row
of tabs, which isn't really the important part of the application, the terminal
is.

Additionally, if there were top-level panes, once the root was split, it would
not be possible to move a single pane to be the full size of the window. The
user would need to somehow close the other panes, to be able to make the split
the size of the dull window.

One con of this design is that if a control is hosted in a pane, the current
design makes it hard to move out of a pane into it's own tab, or into another
pane. This could be solved a number of ways. There could be keyboard shortcuts
for swapping the positions of tabs, or a shortcut for both "zooming" a tab
(temporarily making it the full size) or even popping a pane out to it's own
tab. Additionally, a right-click menu option could be added to do the
aformentioned actions. Discoverability of these two actions is not as high as
just dragging a tab from one pane to another; however, it's believed that panes
are more of a power-user scenario, and power users will not necessarily be
turned off by the feature's discoverability.
