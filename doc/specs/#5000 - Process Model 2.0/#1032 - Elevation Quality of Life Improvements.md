---
author: Mike Griese @zadjii-msft
created on: 2020-11-20
last updated: 2020-12-07
issue id: #1032
---

# Elevation Quality of Life Improvements

## Abstract

For a long time, we've been researching adding support to the Windows Terminal
for running both unelevated and elevated (admin) tabs side-by-side, in the same
window. However, after much research, we've determined that there isn't a safe
way to do this without opening the Terminal up as a potential
escalation-of-privilege vector.

Instead, we'll be adding a number of features to the Terminal to improve the
user experience of working in elevated scenarios. These improvements include:

* A visible indicator that the Terminal window is elevated ([#1939])
* Configuring the Terminal to always run elevated ([#632])
* Configuring a specific profile to always open elevated ([#632])
* Allowing new tabs, panes to be opened elevated directly from an unelevated
  window
* Dynamic profile appearance that changes depending on if the Terminal is
  elevated or not. ([#1939], [#8311])

## Background

_This section was originally authored in the [Process Model 2.0 Spec]. Please
refer to it there for its original context._

Let's presume that you're a user who wants to be able to open an elevated tab
within an otherwise unelevated Terminal window. We call this scenario "mixed
elevation" - the tabs within the Terminal can be running either unelevated _or_
elevated client applications.

It wouldn't be terribly difficult for the unelevated Terminal to request the
permission of the user to spawn an elevated client application. The user would
see a UAC prompt, they'd accept, and then they'd be able to have an elevated
shell alongside their unelevated tabs.

However, this creates an escalation of privilege vector. Now, there's an
unelevated window which is connected directly to an elevated process. At this
point, **any other unelevated application could send input to the Terminal's
`HWND`**. This would make it possible for another unelevated process to "drive"
the Terminal window, and send commands to the elevated client application.

It was initially theorized that the window/content model architecture would also
help enable "mixed elevation". With mixed elevation, tabs could run at different
integrity levels within the same terminal window. However, after investigation
and research, it has become apparent that this scenario is not possible to do
safely after all. There are numerous technical difficulties involved, and each
with their own security risks. At the end of the day, the team wouldn't be
comfortable shipping a mixed-elevation solution, because there's simply no way
for us to be confident that we haven't introduced an escalation-of-privilege
vector utilizing the Terminal. No matter how small the attack surface might be,
we wouldn't be confident that there are _no_ vectors for an attack.


Some things we considered during this investigation:

* If a user requests a new elevated tab from an otherwise unelevated window, we
  could use UAC to create a new, elevated window process, and "move" all the
  current tabs to that window process, as well as the new elevated client. Now,
  the window process would be elevated, preventing it from input injection, and
  it would still contains all the previously existing tabs. The original window
  process could now be discarded, as the new elevated window process will
  pretend to be the original window.
  - However, it is unfortunately not possible with COM to have an elevated
    client attach to an unelevated server that's registered at runtime. Even in
    a packaged environment, the OS will reject the attempt to `CoCreateInstance`
    the content process object. this will prevent elevated windows from
    re-connecting to unelevated client processes.
  - We could theoretically build an RPC tunnel between content and window
    processes, and use the RPC connection to marshal the content process to the
    elevated window. However, then _we_ would need to be responsible for
    securing access the the RPC endpoint, and we feel even less confident doing
    that.
  - Attempts were also made to use a window-broker-content architecture, with
    the broker process having a static CLSID in the registry, and having the
    window and content processes at mixed elevation levels `CoCreateInstance`
    that broker. This however _also_ did not work across elevation levels. This
    may be due to a lack of Packaged COM support for mixed elevation levels.
    Even if this approach did end up working, we would still need to be
    responsible for securing the elevated windows so that an unelevated attacker
    couldn't hijack a content process and trigger unexpected code in the window
    process. We didn't feel confident that we could properly secure this channel
    either.

## Solution Design

Instead of supporting mixed elevation in the same window, we'll introduce the
following features to the Terminal. These are meant as a way of improving the
quality of life for users who work in mixed-elevation (or even just elevated)
environments.

### Visible indicator for elevated windows

As requested in #1939, it would be nice if it was easy to visibly identify if a
Terminal window was elevated or not.

One easy way of doing this is by adding a simple UAC shield to the left of the
tabs for elevated windows. This shield could be configured by the theme (see
[#3327]). We could provide the following states:
* Colored (the default)
* Monochrome
* Hidden, to hide the shield even on elevated windows. This is the current
  behavior.

![UAC-shield-in-titlebar](UAC-shield-in-titlebar.png)
_figure 1: a monochrome UAC shield in the titlebar of the window, courtesy of @mdtauk_

### Change profile appearance for elevated windows

In [#3062] and [#8345], we're planning on allowing users to set different
appearances for a profile whether it's focused or not. We could do similar thing
to enable a profile to have a different appearance when elevated. In the
simplest case, this could allow the user to set `"background": "#ff0000"`. This
would make a profile always appear to have a red background when in an elevated
window.

The more specific details of this implementation are left to the spec
[Configuration object for profiles].

### Configuring a profile to always run elevated

Oftentimes, users might have a particular tool chain that only works when
running elevated. In these scenarios, it would be convenient for the user to be
able to identify that the profile should _always_ run elevated. That way, they
could open the profile from the dropdown menu of an otherwise unelevated window
and have the elevated window open with the profile automatically.

We'll be adding the `"elevate": true|false` setting as a per-profile setting,
with a default value of `false`. When set to `true`, we'll try to auto-elevate
the profile whenever it's launched. We'll check to see if this window is
elevated before creating the connection for this profile. If the window is not
elevated, then we'll create a new window with the requested elevation level to
handle the new connection.

`"elevate": false` will do nothing. If the window is already elevated, then the
profile won't open an un-elevated window.

If the user tries to open an `"elevated": true` profile in a window that's
already elevated, then a new tab/split will open in the existing window, rather
than spawning an additional elevated window.

There are three situations where we're creating new terminal instances: new
tabs, new splits, and new windows. Currently, these are all actions that are
also exposed in the `wt` commandline as subcommands. We can convert from the
commandline arguments into these actions already. Therefore, it shouldn't be too
challenging to convert these actions back into the equal commandline arguments.

For the following examples, let's assume the user is currently in an unelevated
Terminal window.

When the user tries to create a new elevated **tab**, we'll need to create a new
process, elevated, with the following commandline:

```
wt new-tab [args...]
```

When we create this new `wt` instance, it will obey the glomming rules as
specified in [Session Management Spec]. It might end up glomming to another
existing window at that elevation level, or possibly create its own window.

Similarly, for a new elevated **window**, we can make sure to pass the `-s -1`
arguments to `wt`. These parameters indicate that we definitely want this
command to run in a new window, regardless of the current glomming settings.

```
wt -s -1 new-tab [args...]
```

However, creating a new **pane** is a little trickier. Invoking the `wt
split-pane [args...]` is straightforward enough.

⚠ **TODO**: For discussion:

If the current window doesn't have the same elevation level as the
requested profile, do we always want to just create a new split? If the command
ends up glomming to an existing window, does that even make sense? That invoking
an elevated split in an unelevated window would end up splitting the elevated
window? It's very possible that the user wanted a split in the tab they're
currently in, in the unelevated window, but they don't want a split in the
elevated window.

What if there's not space in the elevated window to create the split (but there
would be in the current window)? That would sure make it seem like nothing
happened, silently.

We could alternatively have cross-elevation splits default to always opening a
new tab. That might mitigate some of the odd behaviors. Until we actually have
support for running commands in existing windows, we'll always need to make a
new window when running elevated. We'll need to make the new window for new tabs
and splits, because there's no way to invoke another existing window.

A third proposal is to pop a warning dialog at the user when they try to open an elevated split from and unelevated window. This dialog could be something like

> What you requested couldn't be completed as non-sense do you want to:
> A. Make me a new tab instead.
> B. Forget it and cancel. I'll go fix my config.

I'm certainly leaning towards proposal 2 - always create a new tab. This is how
it's implemented in [#8514]. In that PR, this seems to work resonably sensibly.

#### Configure the Terminal to _always_ run elevated

`elevate` is a per-profile property, not a global property. If a user
wants to always have all instances of the Terminal run elevated, they
could set `"elevate": true` in their profile defaults. That would cause _all_
profiles they launch to always spawn as elevated windows.

#### `elevate` in Actions

Additionally, we'll add the `elevate` property to the `NewTerminalArgs` used in
the `newTab`, `splitPane`, and `newWindow` actions. This is similar to how other
properties of profiles can be overridden at launch time. This will allow
windows, tabs and panes to all be created specifically as elevated windows.

In the `NewTerminalArgs`, `elevate` will be an optional boolean, with the
following behavior:
* `null` (_default_): Don't modify the `elevate` property for this profile
* `true`: This launch should act like the profile had `"elevate": true` in its
  properties.
* `false`: This launch should act like the profile had `"elevate": false` in its
  properties.

We'll also add an iterable command for opening a profile in an
elevated tab, with the following json:

```jsonc
{
    // New elevated tab...
    "name": { "key": "NewElevatedTabParentCommandName", "icon": "UAC-Shield.png" },
    "commands": [
        {
            "iterateOn": "profiles",
            "icon": "${profile.icon}",
            "name": "${profile.name}",
            "command": { "action": "newTab", "profile": "${profile.name}", "elevated": true }
        }
    ]
},
```

#### Elevation from the dropdown

Currently, the new tab dropdown supports opening a new pane by
<kbd>Alt+click</kbd>ing on a profile. We could similarly add support to open a
tab elevated with <kbd>Ctrl+click</kbd>. This is similar to the behavior of the
Windows taskbar. It supports creating an elevated instance of a program by
<kbd>Ctrl+click</kbd>ing on entries as well.

## Implementation Details

### Starting an elevated process from an unelevated process

It seems that we're able to create an elevated process by passing the `"runas"`
verb to
[`ShellExecute`](https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutea).
So we could use something like

```c++
ShellExecute(nullptr,
             L"runas",
             L"wt.exe",
             L"-s -1 new-tab [args...]",
             nullptr,
             SW_SHOWNORMAL);
```

This will ask the shell to perform a UAC prompt before spawning `wt.exe` as an
elevated process.

> 👉 NOTE: This mechanism won't always work on non-Desktop SKUs of Windows. For
> more discussion, see [Elevation on OneCore SKUs](#Elevation-on-OneCore-SKUs).

## Potential Issues

<table>
<tr>
<td><strong>Accessibility</strong></td>
<td>

The set of changes proposed here are not expected to introduce any new
accessibility issues. Users can already create elevated Terminal windows. Making
it easier to create these windows doesn't really change our accessibility story.

</td>
</tr>
<tr>
<td><strong>Security</strong></td>
<td>

We won't be doing anything especially unique, so there aren't expected to be any
substantial security risks associated with these changes. Users can already
create elevated Terminal windows, so we're not really introducing any new
functionality, from a security perspective.

We're relying on the inherent security of the `runas` verb of `ShellExecute` to
prevent any sort of unexpected escalation-of-privilege.

</td>
</tr>
<tr>
<td><strong>Reliability</strong></td>
<td>

No changes to our reliability are expected as a part of this change.

</td>
</tr>
<tr>
<td><strong>Compatibility</strong></td>
<td>

There are no serious compatibility concerns expected with this changelist. The
new `elevated` property will be unset by default, so users will heed to opt-in
to the new auto-elevating behavior.

There is one minor concern regarding introducing the UAC shield on the window.
We're planning on using themes to configure the appearance of the shield. That
means we'll need to ship themes before the user will be able to hide the shield
again.

</td>
</tr>
<tr>
<td><strong>Performance, Power, and Efficiency</strong></td>
<td>

No changes to our performance are expected as a part of this change.

</td>
</tr>
</table>

### Centennial Applications

In the past, we've had a notoriously rough time with the Centennial app
infrastructure and running the Terminal elevated. Notably, we've had to list all
our WinRT classes in our SxS manifest so they could be activated using
unpackaged WinRT while running elevated. Additionally, there are plenty of
issues running the Terminal in an "over the shoulder" elevation (OTS) scenario.

Specifically, we're concerned with the following scenario:
  * the current user account has the Terminal installed,
  * but they aren't an Administrator,
  * the Administrator account doesn't have the Terminal installed.

In that scenario, the user can run into issues launching the Terminal in an
elevated context (even after entering the Admin's credentials in the UAC
prompt).

This spec proposes no new mitigations for dealing with these issues. It may in
fact make them more prevalent, by making elevated contexts more easily
accessible.

Unfortunately, these issues are OS bugs that are largely out of our own control.
We will continue to apply pressure to the centennial app team internally as we
encounter these issues. They are are team best equipped to resolve these issues.

### Default Terminal & auto-elevation

In the future, when we supporting setting the Terminal as the "default terminal
emulator" on Windows. When that lands, we will use the `profiles.defaults`
settings to create the tab we'll hosting the commandline client. If the user has
`"elevate": true` in their `profiles.defaults`, we'd usually try to
auto-elevate the profile. In this scenario, however, we can't do that. The
Terminal is being invoked on behalf of the client app launching, instead of the
Terminal invoking the client application.

### Elevation on OneCore SKUs

This spec proposes using `ShellExecute` to elevate the Terminal window. However,
not all Windows SKUs have support for `ShellExecute`. Notably, the non-Desktop
SKUs, which are often referred to as "OneCore" SKUs. On these platforms, we
won't be able to use `ShellExecute` to elevate the Terminal. There might not
even be the concept of multiple elevation levels, or different users, depending
on the SKU.

Fortunately, this is a mostly hypothetical concern for the moment. Desktop is
the only publicly supported SKU for the Terminal currently. If the Terminal ever
does become available on those SKUs, we can use these proposals as mitigations.

* If elevation is supported, there must be some other way of elevating a
  process. We could always use that mechanism instead.
* If elevation isn't supported (I'm thinking 10X is one of these), then we could
  instead display a warning dialog whenever a user tries to open an elevated
  profile.
  - We could take the warning a step further. We could add another settings
    validation step. This would warn the user if they try to mark any profiles
    or actions as `"elevate":true`

## Future considerations

* If we wanted to go even further down the visual differentiation route, we
  could consider allowing the user to set an entirely different theme ([#3327])
  based on the elevation state. Something like `elevatedTheme`, to pick another
  theme from the set of themes. This would allow them to force elevated windows
  to have a red titlebar, for example.
* Over the course of discussion concerning appearance objects ([#8345]), it
  became clear that having separate "elevated" appearances defined for
  `profile`s was overly complicated. This is left as a consideration for a
  possible future extension that could handle this scenario in a cleaner way.

### De-elevating a Terminal

the original version of this spec proposed that `"elevated":false` from an
elevated Terminal window should create a new unelevated Terminal instance. The
mechanism for doing this is described in [The Old New Thing: How can I launch an
unelevated process from my elevated process, redux].

This works well when the Terminal is running unpackaged. However, de-elevating a
process does not play well with packaged centennial applications. When asking
the OS to run the packaged application from an elevated context, the system will
still create the child process _elevated_. This means the packaged version of
the Terminal won't be able to create a new unelevated Terminal instance.

If this is fixed in the future, we could theoretically re-introduce de-elevating
a profile. The original spec propsed a `"elevated": bool?` setting, with the
following behaviors:
* `null` (_default_): Don't modify the elevation level when running this profile
* `true`: If the current window is unelevated, try to create a new elevated
  window to host this connection.
* `false`: If the current window is elevated, try to create a new unelevated
  window to host this connection.

We could always re-introduce this setting, to superceed `elevate`.

<!-- Footnotes -->

[#1939]: https://github.com/microsoft/terminal/issues/1939
[#8311]: https://github.com/microsoft/terminal/issues/8311
[#3062]: https://github.com/microsoft/terminal/issues/3062
[#8345]: https://github.com/microsoft/terminal/issues/8345
[#3327]: https://github.com/microsoft/terminal/issues/3327
[#5000]: https://github.com/microsoft/terminal/issues/5000
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632
[#8514]: https://github.com/microsoft/terminal/issues/8514
[Process Model 2.0 Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%235000%20-%20Process%20Model%202.0.md
[Configuration object for profiles]: https://github.com/microsoft/terminal/blob/main/doc/specs/Configuration%20object%20for%20profiles.md
[Session Management Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%234472%20-%20Windows%20Terminal%20Session%20Management.md
[The Old New Thing: How can I launch an unelevated process from my elevated process, redux]: https://devblogs.microsoft.com/oldnewthing/20190425-00/?p=102443

