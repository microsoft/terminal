---
author: Mike Griese @zadjii-msft
created on: 2020-11-20
last updated: 2020-11-20
issue id: #1032
---

# Elevation Quality of Life Improvements

## Abstract

For a long time, we've been researching adding support to the Windows Terminal
for running bot unelevated and elevated (admin) tabs side-by-side, in the same
window. However, after much research, we've determined that there isn't a
resonably safe way to do this without opening the Terminal up as a potential
escalation-of-privledge vector.

Instead, we'll be adding a number of features to the Terminal in order to
improve the user experience of working in elevated scenarios. These improvements
include:

* A visible indicator that the Terminal window is elevated ([#1939])
* Configuring the Terminal to always run elevated ([#632])
* Configuring a specific profile to always open elevated ([#632])
* Allowing new tabs, panes to be opened elevated directly from an unelevated
  window
* Dynamic profile appearance that changes depending on if the Terminal is
  elevated or not.  ([#1939], [#8311])

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
`HWND`**, making it possible for another unelevated process to "drive" the
Terminal window and send commands to the elevated client application.

It was initially theorized that the window/content model architecture would also
help enable "mixed elevation", where there are tabs running at different
integrity levels within the same terminal window. However, after investigation
and research, it has become apparent that this scenario is not possible to do
safely after all. There are numerous technical difficulties involved, and each
with their own security risks. At the end of the day, the team wouldn't be
comfortable shipping a mixed-elevation solution, because there's simply no way
for us to be confident that we haven't introduced an escalation-of-privledge
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
    couldn't hijack a content process and trigger unexepected code in the window
    process. We didn't feel confident that we could properly secure this channel
    either.

## Solution Design

Instead of supporting mixed elevation in the same window, we'll introduce the
following features to the Terminal. These are meant as a way of inproving the
quality of life for users who work in mixed-elevation (or even just elevated)
environments.

### Visible indicator for elevated windows

As requested in #1939, it would be nice if it was easy to visibly identify if a
Terminal window was elevated or not.

One easy way of doing this is by adding a simple UAC shield to the left of the
tabs for elevated windows. This shield could be configured by the theme (see
[#3327]) to be either colored (the default), monochrome, or hidden (if the user
really doesn't want the shield visible on elevated windows).

![UAC-shield-in-titlebar](UAC-shield-in-titlebar.png)
_figure 1: a monochrome UAC shield in the titlebar of the window, courtesy of @mdtuak_

### Change profile appearance for elevated windows

In [#3062] and [#8345], we're planning on allowing users to set different
appearances for a profile whether it's focused or not. We could do similar thing
to enable a profile to have a different appearance when elevated. In the
simplest case, this could allow the user to set `"background": "#ff0000"` to
make a profile always appear to have a red background when in an elevated
window.

### Configuring a profile to always run elevated

Oftentimes, users might have a particular toolchain that only works when running
elevated. In these scenarios, it would be convenient for the user to be able to
identify that the profile should _always_ run elevated. That way, they could
open the profile from the dropdown menu of an otherwise unelevated window and
have the elevated window open with the profile automatically.

We'll be adding the `"elevated": true|false` setting as an _optional_ per-profile setting, with a default value _not being set_. When set to `true` or `false`, we'll check to see if this window is elevated before creating the connection for this profile. If the window has a different elevation level than requested in the profile, then we'll create a new window with the requested elevation level to handle the new connection.

If the user tries to open an `"elevated": true` profile in a window that's
already elevated, then a new tab/split will open in the existing window, rather
than spawning an additional elevated window.

There are three situations where we're creating new terminal instances: new tabs, new splits, and new windows.

When the user tries to create a new tab that wants to be elevated, and the current window isn't  elevated, we'll need to make a new process, elevated:

```
wt new-tab [args...]
```

That will automatically follow the glomming rules as specified in [Session Management Spec]

Similarly, for splitting a pane
```
wt split-pane [args...]
```
That'll glom to the correct window, and create the split in that window. That might be unintuitive though, we might want to stick with creating new tabs.


A new window is
```
wt -s -1 new-tab [args...]
```


[TODO]: TODO! ------------------------------------------------------------------

Wait, what if they want to split a pane, but then the window is unelevated, and they want to glom onto the existing elevated window?

We'll spawn a elevated `wt split-pane [args...]`, and then that

## Implementation Details

### Starting an elevated process from an unelevated process

### Starting an unelevated process from an elevated process


## UI/UX Design

## Concerns

<table>
<tr>
<td><strong>Accessibility</strong></td>
<td>

</td>
</tr>
<tr>
<td><strong>Security</strong></td>
<td>


</td>
</tr>
<tr>
<td><strong>Reliability</strong></td>
<td>


</td>
</tr>
<tr>
<td><strong>Compatibility</strong></td>
<td>

</td>
</tr>
<tr>
<td><strong>Performance, Power, and Efficiency</strong></td>
<td>

</td>
</tr>
</table>


## Potential Issues

## Implementation Plan



## Footnotes

<a name="footnote-1"><a>[1]:

## Future considerations

* If we wanted to go even further down the visual differentiation route, we
  could consider allowing the user to set an entirely different theme ([#3327])
  based on the elevation state. Something like `elevatedTheme`, to pick another
  theme from the set of themes. This would allow them to force elevated windows
  to have a red titlebar, for example.

## Resources

* [Tab Tear-out in the community toolkit] - this document proved invaluable to
  the background of tearing a tab out of an application to create a new window.

<!-- Footnotes -->

[#1939]: https://github.com/microsoft/terminal/issues/1939
[#8311]: https://github.com/microsoft/terminal/issues/8311
[#3062]: https://github.com/microsoft/terminal/issues/3062
[#8345]: https://github.com/microsoft/terminal/issues/8345
[#3327]: https://github.com/microsoft/terminal/issues/3327
[#5000]: https://github.com/microsoft/terminal/issues/5000
[#1256]: https://github.com/microsoft/terminal/issues/1256
[#4472]: https://github.com/microsoft/terminal/issues/4472
[#2227]: https://github.com/microsoft/terminal/issues/2227
[#653]: https://github.com/microsoft/terminal/issues/653
[#1032]: https://github.com/microsoft/terminal/issues/1032
[#632]: https://github.com/microsoft/terminal/issues/632
[#492]: https://github.com/microsoft/terminal/issues/492
[#4000]: https://github.com/microsoft/terminal/issues/4000
[#7972]: https://github.com/microsoft/terminal/pull/7972
[#961]: https://github.com/microsoft/terminal/issues/961
[`30b8335`]: https://github.com/microsoft/terminal/commit/30b833547928d6dcbf88d49df0dbd5b3f6a7c879
[#8135]: https://github.com/microsoft/terminal/pull/8135
[Process Model 2.0 Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%235000%20-%20Process%20Model%202.0.md
[Session Management Spec]: https://github.com/microsoft/terminal/blob/main/doc/specs/%234472%20-%20Windows%20Terminal%20Session%20Management.md

https://github.com/microsoft/terminal/blob/dev/migrie/s/4472-window-management/doc/specs/%235000%20-%20Process%20Model%202.0/%234472%20-%20Windows%20Terminal%20Session%20Management.md?rgh-link-date=2020-11-02T20%3A55%3A22Z
