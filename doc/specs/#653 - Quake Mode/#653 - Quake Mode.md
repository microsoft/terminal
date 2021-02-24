---
author: Mike Griese @zadjii-msft
created on: 2021-02-23
last updated: 2021-02-23
issue id: #653
---

# Quake Mode

## Abstract

Many existing terminals support a feature whereby a user can press a keybinding
anywhere in the OS, and summon their terminal application. Oftentimes the act of
summoning this window is accompanied by a "dropdown" animation, where the window
slides in to view from the top of the screen. This global summon action is often
referred to as "quake mode", a reference to the videogame Quake who's console
slid in from the top.

This spec addresses both of the following two issues:
* "Quake Mode" ([#653])
* "Minimize to tray" ([#5727])

Readers should make sure to have read the [Process Model 2.0 Spec], for
background on Monarch and Peasant processes.

## Background

### Inspiration

[comment]: # Are there existing precedents for this type of configuration? These may be in the Terminal, or in other applications

For an example of the original Quake console in action, take a look at the following video (noisy video warning): [Quake 3 sample].

* guake implements this as... TODO
* yakuake implements this by ... TODO

Both these terminals are listed in #653 with descriptions of their specifics. I should include them here.

### User Stories

[comment]: # List off the use cases where two users might want a feature to have different behavior based on user preference. Include links to issues that might be relevant.

The original quake mode thread (#653) is absolutely _filled_ with variations on
how users want to be able to summon their terminal windows. These include, but
are not limited to:

* **Story A** Press a hotkey anywhere to activate the single Terminal window
  wherever it was
* **Story B** Press a hotkey anywhere to activate the single Terminal window _on
  the current monitor_. If it wasn't previously on that monitor, move it there.
* **Story C** When the Terminal is summoned using the hotkey, have it "slide in"
  from the top. Similarly, slide out on deactivate.
* **Story D** <kbd>Ctrl+1</kbd> to activate the terminal on monitor 1,
  <kbd>Ctrl+2</kbd> to activate the terminal on monitor 2.
* **Story E** Variable dropdown speed
* **Story F** Minimize to tray, press a hotkey to activate the terminal window
  (#5727)
* **Story G** Terminal doesn't appear in alt+tab view, press a hotkey to
  activate the single terminal window / the nearest terminal window (I'm not
  sure this is distinct from the above)
* **Story H** Press a hotkey to activate the "nearest" terminal window.
  - TODO: I'm not totally sure that anyone wants this one tbh.


### Future Considerations

I don't believe there are any other related features that are planned that
aren't already included in this spec.

[TODO]: # Which is the "current" monitor? The one with the mouse or the one with the active window? Users disagree! Users want it configurable! Gah! Do we make it `onMonitor: onCurrent|onCurrentWindow|forCurent|<int>`


#### Hide from alt-tab

[TODO]: # TODO

This is another request that's all over [#653]. It's unclear if this is the same as minimize to tray. I'm not sure that there's anyone who wants the terminal to `minimizeToTray`, but then _not_ hide from the alt+tab list.

## Solution Design

To implement this feature, we'll add the following settings:
* a new action, named `globalSummon`.
* a new global, app setting named `minimizeToTray`
* a new global, app setting named `alwaysShowTrayIcon`

### `globalSummon` Action

[comment]: # Also, outline various different proposed designs for this setting. These won't all be winners, but may help during the decision making process. For each proposed design:

The `globalSummon` action will be a keybinding the user can use to summon a
Terminal window from anywhere in the OS. Various arguments to the action will
specify which window is summoned, to where, and how the window should behave on
summon.

From a technical perspective, the action will work by using the
[`RegisterHotKey`] function. This API allows us to bind a particular hotkey with
the OS. Whenever that hotkey is pressed, our window message loop will recieve a
`WM_HOTKEY`.

Since users may want to bind multiple keys to summon different windows, we'll
need to allow the user to specify multiple keybindings simultaneously, each with
their own set of args. The following are three proposals for different ways for
allowing users to specify multiple global hotkeys:

<hr>

##### Proposal 1A

We stick all the `globalSummon`s in the actions array, like they're any other
keybinding.

However, these are not keys that are handled by the TerminalApp layer itself.
These are keys that need to be registered with the OS. So they don't make sense
to put in the normal `KeyMap`. We'll need to parse them not into the `KeyMap`,
but into a global summon list that the window layer can get at easier.

**Pros**:
* We re-use the existing _action_ arg parsing entirely.
* These actions would show up as commands for the command palette.
  - I don't know if that's desirable. Would we want them to? They'd need to
    raise events into the window layer, for the window to dispatch them.
  - If it is desirable, then these actions could also be added to the context
    menu, the new tab dropdown, anywhere we plan on placing actions.

**Cons**:
* We'll need to modify the keybinding parsing to place globalSummon actions into
  a totally different object
* Not sure that having these available in the command palette really makes sense

##### Proposal 1B

If we're going to need another data structure (separate from the `KeyMap`), then
why have the `globalSummon` actions in `actions` at all? Instead, we could add a
top-level `globalHotkeys` array, with only global summon actions in it.

```json
{
    "globalHotkeys": [
        { "keys": "win+1", "command": { "action": "globalSummon", "monitor": 1 } },
        { "keys": "win+2", "command": { "action": "globalSummon", "monitor": 2, "desktop": "toCurrent" } }
    ]
}
```

**Pros**:
* We re-use the existing _action_ arg parsing entirely.
* Don't need to modify (complicate) the current keybinding parsing.
* Clearly separates these actions from the bulk of other actions

**Cons**:
* Is fairly verbose to add to the settings. `"command": { "action":
  "globalSummon"` is present in _all_ of the actions, but doesn't add value.
* Difficult, but not impossible, to get these actions into other places in the
  UI that we want actions (unsure if this is even wanted)

##### Proposal 1C

The previous proposal is qute verbose. We've already got these actions in their
own setting. We could instead add a top-level `globalHotkeys` array, but with a
less verbose syntax. Everything in here is a global summon action, no need to
repeat ourselves:

```json
{
    "globalHotkeys": [
        { "keys": "win+1", "monitor": 1 },
        { "keys": "win+2", "monitor": 2, "desktop": "toCurrent" }
    ]
}
```

**Pros**:
* Is the least verbose way of adding these actions.
* Don't need to modify (complicate) the current keybinding parsing.
* Clearly separates these actions from the bulk of other actions.

**Cons**:
* We're going to need a new _action_ parser just for global hotkeys like this.
  One that always tries the action as a `globalSummon` action.
* **H**ard to get these actions into other places in the UI that we want actions
  (unsure if this is even wanted)

##### Conclusion

From an engineering standpoint, I like 1C the best. It doesn't complicate the
existing keybinding parsing. The cost of modifying the existing action arg
parsing just for `globalSummon` actions shouldn't be too high. It's also the
most ergonomic for users to add this fairly complex setting.

<hr>

#### Which window, and where?

When looking at the list of requested scenarios, there are lots of different
ways people would like to use the global summon action. Some want the most
recent window activated, always. Others want to have one window _per monitor_.
Some would like to move the window to where the user is currently interacting
with the PC, and others want to activate the window where it already exists.
Trying to properly express all these possible configurations is complex. The
settings should be unambiguous as to what will happen when you press the
keybinding.

I believe that in order to accurately support all the variations that people
might want, we'll need two properties in the `globalSummon` action. These
properties will specify _which_ window we're summoning, and _where_ to summon
the window. To try and satisfy all these scenarios, I'm proposing the following
two arguments to the `globalSummon` action:

```json
"monitor": "any"|"toCurrent"|"onCurrent"|int,
"desktop": null|"toCurrent"|"onCurrent"
```

The way these settings can be combined is in a table below. As an overview:

* `monitor`: This controls the monitor that the window will be summoned from/to
  - `"any"`/omitted: (_default_) Summon the MRU window, regardless of which
    monitor it's currently on.
  - `"toCurrent"`: Summon the MRU window TO the current monitor.
  - `"onCurrent"`: Summon the MRU window ALREADY ON the current monitor.
  - `int`: Summon the MRU window for the given monitor (as identified by the
    "Identify" displays feature in the OS settings)

* `desktop`: This controls how the terminal should interact with virtual desktops.
  - `null`/omitted: (_default_) Leave the window on whatever desktop it's
    already on - we'll switch to that desktop as we activate the window.
    - **TODO: FOR DISCUSSION**: Should this just be `any` to match the above?
        Read the rest of the doc and come back.
  - `"toCurrent"`: Move the window to the current virtual desktop
  - `"onCurrent"`: Only summon the window if it's already on the current virtual
    desktop

Together, these settings interact in the following ways:

<table>

<tr>
<td></td>
<td><strong>Leave where it is</strong></td>
<td><strong>Move to current desktop</strong></td>
<td><strong>On current desktop only</strong></td>
</tr>

<tr>
<td>Summon the MRU window</td>

<td>Go to the desktop the window is on  (leave position alone)</td>
<td>Move the window to this desktop (leave position alone)</td>
<td>

If there isn't one on this desktop:
* create a new one (default position)

Else:
* activate the one on this desktop (don't move it)
</td>
</tr>
<!-- ----------------------------------------------------------------------- -->
<tr>
<td>Summon the MRU window TO the current monitor</td>

<td>Go to the desktop the window is on, move to this monitor</td>
<td>Move the window to this desktop, move to this monitor</td>
<td>

If there isn't one on this desktop:
* create a new one (on this monitor)

Else:
* activate the one on this desktop, move to this window
</td>
</tr>
<!-- ----------------------------------------------------------------------- -->
<tr>
<td>Summon the MRU window for the current monitor</td>

<td>

If there is a window on this monitor on any desktop,
* Go to the desktop the window is on (leave position alone)

else
* Create a new window on this monitor  & desktop
</td>
<td>

If there is a window on this monitor on any desktop,
* Move the window to this desktop (leave position alone)

else
* Create a new window on this monitor  & desktop
</td>
<td>

If there isn't one on this desktop, (even if there is one on this monitor on another desktop),
* create a new one on this monitor

Else if ( there is one on this desktop, not this monitor)
* create a new one on this monitor

Else (one on this desktop & monitor)
* Activate the one on this desktop (don't move)
</td>
</tr>
<!-- ----------------------------------------------------------------------- -->
<tr>
<td>Summon the MRU window for monitor N</td>

<td>

If there is a window on monitor N on any desktop,
* Go to the desktop the window is on (leave position alone)

else
* Create a new window on this monitor  & desktop
</td>
<td>

If there is a window on monitor N on any desktop,
* Move the window to this desktop (leave position alone)

else
* Create a new window on this monitor & desktop
</td>
<td>

If there isn't one on this desktop, (even if there is one on monitor N on another desktop),
* create a new one on monitor N

Else if ( there is one on this desktop, not monitor N)
* create a new one on monitor N

Else (one on this desktop & monitor N)
* Activate the one on this desktop (don't move)
</td>
</tr>

</table>


##### Stories, revisited

With the above settings, let's re-examine the original user stories, and see how
they fit into the above settings. (_Stories that are omitted aren't relevant to
the discussion of these settings_)

> When the `desktop` param is omitted below, that can be interpreted as "any
> `desktop` value will make sense here"

* **Story A** Press a hotkey anywhere to activate the single Terminal window
  wherever it was
  - This is `{ "monitor": "any", "desktop": null }`
* **Story B** Press a hotkey anywhere to activate the single Terminal window _on
  the current monitor_. If it wasn't previously on that monitor, move it there.
  - This is `{ "monitor": "toCurrent" }`
* **Story D** <kbd>Ctrl+1</kbd> to activate the terminal on monitor 1,
  <kbd>Ctrl+2</kbd> to activate the terminal on monitor 2.
  - This is `[ { "keys": "ctrl+1", monitor": 1 }, { "keys": "ctrl+2", monitor": 2 } ]`

As some additional examples:

```json
// Go to the MRU window, wherever it is
{ "keys": "win+1", "monitor":"any", "desktop": null },
// Since "any" & null are the default values, just placing a single entry here
// will bind the same behavior:
{ "keys": "win+1" },

// activate the MRU window, and move it to this desktop & this monitor
{ "keys": "win+2", "monitor":"toCurrent", "desktop": "toCurrent" },

// activate the MRU window on this desktop
{ "keys": "win+3", "monitor":"any", "desktop": "onCurrent" },

// Activate the MRU window on monitor 2 (from any desktop), and place it on the
// current desktop. If there isn't one on monitor 2, make a new one.
{ "keys": "win+4", "monitor": 2, "desktop": "toCurrent" },

// Activate the MRU window on monitor 3 (ONLY THIS desktop), or make a new one.
{ "keys": "win+5", "monitor": 3, "desktop": "onCurrent" },

// Activate the MRU window on this monitor (from any desktop), and place it on
// the current desktop. If there isn't one on this monitor, make a new one.
{ "keys": "win+6", "monitor": "onCurrent", "desktop": "toCurrent" },
```


### Proposal 1: <name of proposal>

[comment]: # Describe the values for the properties, how it'll be exposed in both JSON and the Settings UI, and list pros and cons for this design. If there are technical details for this proposal, include them here.

* **Pros**:
* **Cons**:

### Minimize to Tray

[TODO]: # TODO: add samples of how it's implemented
[TODO]: # TODO: Find a sample for hiding the window from the taskbar (when minimize to tray is active)

The tray icon could include a list of all the active windows. Maybe we'll have
two items on the tray icon:

```
Focus Terminal
---
Windows > Window 1 - <un-named window>
          Window 2 - "This-window-does-have-a-name"
```

Just clicking on the icon would summon the recent terminal window. Right
clicking would show the menu with "Focus Terminal" and "Windows" in it, and
"Windows" would have nested entries for each Terminal window. Clicking those
would summon them.

The tray notification would be visible always when the user has
`"minimizeToTray": true` set in their settings. If the user has that set to
false, but would still like the tray, they can specify `"alwaysShowTrayIcon":
true`. That will cause the tray icon to always be added to the system tray.

There's not a combination of settings where the Terminal is "minimized to the
tray", and there's _no tray icon visible_. We don't want to let users get into a
state where the Terminal is running, but is totally hidden from their control.

## Conclusion

[comment]: # Of the above proposals, which should we decide on, and why?


## UI/UX Design

[comment]: # How will different values of this setting affect the end user?

## Potential Issues

<table>

<tr>
<td><strong>Compatibility</strong></td>
<td>

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

</td>
</tr>
</table>

[comment]: # If there are any other potential issues, make sure to include them here.


## Resources

Docs on adding a system tray item:
* https://docs.microsoft.com/en-us/windows/win32/shell/notification-area
* https://www.codeproject.com/Articles/18783/Example-of-a-SysTray-App-in-Win32

[comment]: # Be sure to add links to references, resources, footnotes, etc.




### Footnotes

<a name="footnote-1"><a>[1]:


[#653]: https://github.com/microsoft/terminal/issues/653
[#5727]: https://github.com/microsoft/terminal/issues/5727
[Process Model 2.0 Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%235000%20-%20Process%20Model%202.0/%235000%20-%20Process%20Model%202.0.md
[Quake 3 sample]: https://youtu.be/ZmR6HQbuHPA?t=27
[`RegisterHotKey`]: # TODO
