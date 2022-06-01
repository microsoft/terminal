---
author: Mike Griese @zadjii-msft
created on: 2020-11-23
last updated: 2020-12-15
issue id: #2871
---

# Focus Pane Actions

## Abstract

Currently, the Terminal only allows users to navigate through panes
_directionally_. However, we might also want to allow a user to navigate through
panes in most recently used order ("MRU" order), or to navigate directly to a
specific pane. This spec proposes some additional actions in order to enable
these sorts of scenarios.

## Background

### Inspiration

`tmux` allows the user to navigate through panes using its `select-pane`
command. The `select-pane` command works in the following way:

```
select-pane [-DLlMmRU] [-T title] [-t target-pane]

     Make pane target-pane the active pane in window target-window, or set its
     style (with -P).  If one of -D, -L, -R, or -U is used, respectively the
     pane below, to the left, to the right, or above the target pane is used.
     -l is the same as using the last-pane command.

     -m and -M are used to set and clear the marked pane.  There is one marked
     pane at a time, setting a new marked pane clears the last.  The marked pane
     is the default target for -s to join-pane, swap-pane and swap-window.
```
_from `man tmux`_.

The Terminal currently allows the user to navigate through panes with the
`moveFocus` action, which only accepts a `direction` to move in.

Additionally, the Terminal allows movement between tabs with the `nextTab` and
`prevTab` actions, who move between tabs either in-order or in MRU order.
Furthermore, these actions may or may not display the "tab switcher" user
interface, based on the value of `tabSwitcherMode`.

### User Stories

* **Scenario 1**: A user who wants to be able to split the window into 4 equal
  corners from the commandline. Currently this isn't possible, because the user
  cannot move focus during the startup actions - `split-pane` actions always end
  up splitting the current leaf in the tree of panes. (see [#5464])
* **Scenario 2**: A user who wants to quickly navigate to the previous pane they
  had opened. (see [#2871])
* **Scenario 3**: A user who wants to bind a keybinding like <kbd>alt+1</kbd>,
  <kbd>alt+2</kbd>, etc to immediately focus the first, second, etc. pane in a
  tab. (see [#5803])

### Future Considerations

There's been talk of updating the advanced tab switcher to also display panes,
in addition to just tabs. This would allow users to navigate through the ATS
directly to a pane, and see all the panes in a tab. Currently, `tabSwitcherMode`
changes the behavior of `nextTab`, `prevTab` - should we just build the
`paneSwitcherMode` directly into the action we end up designing?

## Solution Design

Does using the pane switcher with a theoretical `focusPane(target=id)` action
even make sense? Certainly not! That's like `switchToTab(index=id)`, the user
already knows which tab they want to go to, there's no reason to pop an
ephemeral UI in front of them.

Similarly, it almost certainly doesn't make sense to display the pane switcher
while moving focus directionally. Consider moving focus with a key bound to the
arrow keys. Displaying another UI in front of them while moving focus with the
arrow keys would be confusing.

Addressing Scenario 1 is relatively easy. So long as we add any of the proposed
actions, including the existing `moveFocus` action as a subcommand that can be
passed to `wt.exe`, then the user should be able to navigate through the panes
they've created with the startup commandline, and build the tree of panes
however they see fit.

Scenario 2 is more complicated, because MRU switching is always more
complicated. Without a UI of some sort, there's no way to switch to another pane
in the MRU order without also updating the MRU order as you go. So this would
almost certainly necessitate a "pane switcher", like the tab switcher.


### Proposal A: Add next, prev to moveFocus

* `moveFocus(direction="up|down|left|right|next|prev")`

* **Pros**:
  - Definitely gets the "MRU Pane Switching" scenario working
* **Cons**:
  - Doesn't really address any of the other scenarios
  - How will it play with pane switching in the UI?
  - MRU switching without a dialog to track & display the MRU stack doesn't
    really work - this only allows to the user to navigate to the most recently
    used pane, or through all the panes in least-recently-used order. This is
    because switching to the MRU pane _will update the MRU pane_.

❌ This proposal is no longer being considered.

### Proposal B: focusNextPane, focusPrevPane with order, useSwitcher args

```json
// Focus pane 1
// - This is sensible, no arguments here
{ "command": { "action": "focusPane", "id": 1 } },

// Focus the next MRU pane
// - Without the switcher, this can only go one pane deep in the MRU stack
// - presumably once there's a pane switcher, it would default to enabled?
{ "command": { "action": "focusNextPane", "order": "mru" } },

// Focus the prev inOrder pane
// - this seems straightforward
{ "command": { "action": "focusPrevPane", "order": "inOrder" } },

// Focus the next pane, in mru order, explicitly disable the switcher
// - The user opted in to only being able to MRU switch one deep. That's fine, that's what they want.
{ "command": { "action": "focusNextPane", "order": "mru", "useSwitcher": false} },

// Focus the prev inOrder pane, explicitly with the switcher
// - Maybe they disabled the switcher globally, but what it on for this action?
{ "command": { "action": "focusPrevPane", "order": "inOrder", "useSwitcher": true } },
```
_From [discussion in the implementation
PR](https://github.com/microsoft/terminal/pull/8183#issuecomment-729672645)_

Boiled down, that's three actions:
* `focusPane(target=id)`
* `focusNextPane(order="inOrder|mru", useSwitcher=true|false)`
* `focusPrevPane(order="inOrder|mru", useSwitcher=true|false)`

* **Pros**:
  - Everything is explicit, including the option to use the pane switcher (when
    available)
  - Adds support for in-order pane switching
  - No "conditional parameters" - where providing one argument makes other
    arguments invalid or ambiguous.
* **Cons**:
  - Doesn't really address any of the other scenarios
  - What does the "next most-recently-used tab" even mean? How is it different
    than "previous most-recently-used tab"? Semantically, these are the same
    thing!
  - No one's even asked for in-order pane switching. Is that a UX that even
    really makes sense?

❌ This proposal is no longer being considered.

> 👉 **NOTE**: At this point, we stopped considering navigating in both MRU
> "directions", since both the next and prev MRU pane are the same thing. We're
> now using "last" to mean "the previous MRU pane".

### Proposal C: One actions, combine the args

* `moveFocus(target=id|"up|down|left|right|last")`

* **Pros**:
  - Absolutely the least complicated action to author. There's only one
    parameter, `target`.
  - No "conditional parameters".
* **Cons**:
  - How do we express this in the Settings UI? Mixed-type enums work fine for
    the font weight, where each enum value has a distinct integer value it maps
    to, but in this case, using `id` is entirely different from the other
    directional values

❌ This proposal is no longer being considered.

### Proposal D: Two actions

* `focusPane(target=id)`
* `moveFocus(direction="up|down|left|right|last")`

* **Pros**:
  - Each action does explicitly one thing.
* **Cons**:
  - two actions for _similar_ behavior
  - This now forks the "Direction" enum into "MoveFocusDirection" and
    "ResizeDirection" (because `resizePane(last)` doesn't make any sense).

This proposal doesn't really have any special consideration for the pane
switcher UX. Neither of these actions would summon the pane switcher UX.

### Proposal E: Three actions

* `focusPane(target=id)`
* `moveFocus(direction="up|down|left|right")`
* `focusLastPane(usePaneSwitcher=false|true)`

In this design, neither `focusPane` nor `moveFocus` will summon the pane
switcher UI (even once it's added). However, the `focusLastPane` one _could_,
and subsequent keypresses could pop you through the MRU stack, while it's
visible? The pane switcher could then display the panes for the tab in MRU
order, and the user could just use the arrow keys to navigate the list if they
so choose.

* **Pros**:
  - Each action does explicitly one thing.
  - Design accounts for future pane switcher UX
* **Cons**:
  - Three separate actions for similar behavior

❌ This proposal is no longer being considered.

### Proposal F: It's literally just tmux

_Also known as the "one action to rule them all" proposal_

`focusPane(target=id, direction="up|down|left|right|last")`

Previously, this design was avoided, because what does `focusPane(target=4,
direction=down)` do? Does it focus pane 4, or does it move focus down?

`tmux` solves this in one action by just doing both!

```
Make pane target-pane the active pane ...  If one of -D, -L, -R, or -U is used,
respectively the pane below, to the left, to the right, or above the target pane
is used.
```
_from `man tmux`_.

So `focusPane(target=1, direction=up)` will attempt to focus the pane above pane
1. This action would not summon the pane switcher UX, even for
`focusPane(direction=last)`

* **Pros**:
  - Fewest redundant actions
* **Cons**:
  - Is this intuitive? That combining the params would do both, with `target`
    happening "first"?
  - Assumes that there will be a separate action added in the future for "Open
    the pane switcher (with some given ordering)"


> 👉 **NOTE**: At this point, the author considered "Do we even want a separate
> action to engage the tab switcher with panes expanded?" Perhaps panes being
> visible in the tab switcher is just part of the tab switcher's behavior. Maybe
> there shouldn't be a separate "open the tab switcher with the panes expanded
> to the pane I'm currently on, and the panes listed in MRU order" action.

❌ This proposal is no longer being considered.

## Conclusion

After much discussion as a team, we decided that **Proposal D** would be the
best option. We felt that there wasn't a need to add any extra configuration to
invoke the "pane switcher" as anything different than the "tab switcher". The
"pane switcher" should really just exist as a part of the functionality of the
advanced tab switcher, not as it's own thing.

Additionally, we concurred that the new "direction" value should be `prev`, not
`last`, for consistency's sake.

## UI/UX Design

The only real UX being added with the agreed upon design is allowing the user to
execute an action to move to the previously active pane within a single tab. No
additional UX (including the pane switcher) is being prescribed in this spec at
this time.

## Potential Issues

<table>
<tr>
<td><strong>Compatibility</strong></td>
<td>

We've only adding a single enum value to an existing enum. Since we're not
changing the meaning of any of the existing values, we do not expect any
compatibility issues there. Additionally, we're not changing the default value
of the `direction` param of the `moveFocus` action, so there are no further
compatibility concerns there. Furthermore, no additional parameters are being
added to the `moveFocus` action that would potentially give it a different
meaning.

</td>
</tr>
</table>

In the current design, there's no way to move through all the panes with a
single keybinding. For example, if a user wanted to bind <kbd>Alt+]</kbd> to
move to the "next" pane, and <kbd>Alt+[</kbd> to move to the "previous" one.
These movements would necessarily need to be in-order traversals, since there's
no way of doing multiple MRU steps.

Fortunately, no one's really asked for traversing the panes in-order, so we're
not really worried about this. Otherwise, it would maybe make sense for `last`
to be the "previous MRU pane", and reserve `next`/`prev` for in-order traversal.


[#2871]: https://github.com/microsoft/terminal/issues/2871
[#5464]: https://github.com/microsoft/terminal/issues/5464
[#5803]: https://github.com/microsoft/terminal/issues/5803


