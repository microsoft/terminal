---
author: Mike Griese @zadjii-msft
created on: 2022-09-07
last updated: 2022-09-07
issue id: 5916
---

# Triggers and Custom Clickable Links

## Abstract

[comment]: # Outline what this spec describes.

## Background

### Inspiration

[comment]: # Are there existing precedents for this type of configuration? These may be in the Terminal, or in other applications

### User Stories

[comment]: # List off the use cases where two users might want a feature to have different behavior based on user preference. Include links to issues that might be relevant.


TODO! Make sure all these threads are addressed:

* [ ] [#5916]
* [ ] [#8849]
* [ ] [#7562]
* [ ] [#8294]
* [ ] [#2671]
* [x] [#6969]
* [ ] [#11901]
* [ ] Probably don't do this for the alt buffer? Make it per-trigger if they work in the alt buffer?

## Solution Design


[TODO!]: # TODO! this was a draft from a comment. Polish.

Could I be even crazier:

```jsonc
{
    "match": "[Ee][Rr][Rr][Oo][Rr]",
    "action": "addMark",
    "color": "#ff0000"
},
{
    "match": "Terminate batch job \\(Y/N\\)",
    "action": "sendInput",
    "input": "y\r\n"
},
{
    "match": "    git push --set-upstream origin ([^\\w]*)",
    "action": "sendInput",
    "input": "git push --set-upstream origin ${match[1]}\r"
},
{
    "match": "'(.*)' is not recognized as an internal or external command,",
    "action": "checkWinGet",
    "commandline": "${match[1]}"
},
{
    "match": "This will open a new Terminal tab",
    "action": "newTab"
},
```

`triggers` is an array of things. First we try to deserialize each thing as a
`Control.Trigger`. There's only a few options, and fundamentally those are
basically actions`.
* `addMark`
* `sendInput`
* `experimental.colorSelection`

And we can _probably_ reuse some of the macros in `ActionArgs.h` to make
deserializing to a `Control.Trigger` easy.

`Control.Trigger`'s, when they are hit by the control, will be able to string
replace their args themselves. The `git push` trigger above - when matched, the
Core will call `Control.SendInputTrigger.Execute(ControlCore core, String[]
matches)` (or something like that). `Execute` will be responsible for
string-replacing anything it needs to.


If We find that the `action` is NOT a `Control.Trigger`, then we'll make it into
an `Settings.Model.Trigger`. We'll hang on to the JSON. When the trigger is hit
by the control, it'll raise an event. The app will recieve the event, match it
up to the trigger, and call `Model.Trigger.Parse(String[] matches) ->
ActionAndArgs` which will do the string replace in the JSON itself. We'll then
just take the _whole json_, and try to parse it literally as an `ActionAndArgs`.

### Turn text into clickable links

A similar request from [#8849] that should also be captured. People want the
ability to configure the regexes that are used for turning text into clickable
links. Currently, we only match on a predefined set<sup>[[1](#footnote-1)]</sup>
into clickable text.

```jsonc
// I did not test these regexes
{
    "match": "(^(.+)\\/([^\\/]+)$):(\\d):(\\d)",
    "action": "clickableLink",
    "target": "code.exe --goto \"${match[1]}:${match[2]}\""
},
{
    "match": "git push --set-upstream origin ([^\\w]*)",
    "action": "clickableLink",
    "target": "vi \"${match[1]}\" +${match[2]}\""
},
{
    "match": "\\b(T\\d+)\\b",
    "action": "clickableLink",
    "target": "https://example.com/tasks/${match[1]}"
},
```

Challenges: 
* with hyperlink matching, we always assume that the cells themselves contain
  the payload to turn into a URL. We'd have to instead find that the text
  matched a trigger, then run it back through the regex to split into matched
  parts, then parse what the target is.

### Disabling triggers

Three main cases:
* If we define a trigger in `defaults.json`, users should be able to disable it.
* If a user puts a trigger into `profiles.defaults`, but doesn't want it to show up in a specific profile
* If a fragment decides to add a trigger to a specific profile

[TODO!]: # TODO!

### Triggers for Terminal Control consumers

Case in point: [#6969]. Visual Studio would like to supply their own trigger,
and recieve an event with the ID of the trigger.

> * Consumers of the terminal must be able to provide a pattern that will identify regions of clickable text
> * Consumers will provide a callback that is called with the clicked text as a parameter

These are addressed generally by the rest of the spec. They could provide
`CommandTriggers` via `ICoreSettings`. When we find the match, we'd raise an
event with the index of that trigger, and the matches for that regex.

## Potential Issues

<table>

<tr><td><strong>Compatibility</strong></td><td>

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

</td></tr>

<tr><td><strong>Accessibility</strong></td><td>

[comment]: # TODO!

</td></tr>

<tr><td><strong>Sustainability</strong></td><td>

[comment]: # TODO!

</td></tr>

<tr><td><strong>Localization</strong></td><td>

[comment]: # TODO!

Mildly worried here about the potential for community-driven tasks to have
non-localized descriptions. We may need to accept a `description:{ en-us:"",
pt-br:"", ...}`-style map of language->string descriptions.

</td></tr>

</table>

[comment]: # If there are any other potential issues, make sure to include them here.

## Implementation Plan

### 🐣 Crawl
* [ ]

### 🚶 Walk
* [ ]

### 🏃‍♂️ Run
* [ ]

### 🚀 Sprint
* [ ]


## Conclusion

[comment]: # Of the above proposals, which should we decide on, and why?


### Future Considerations

#### Clickable sendInput

There's already an existing standard for "this text is a hyperlink" - [OSC8].
That already accepts a list of arbitrary parameters. Without even really
_extending_ that sequence, we could accept a `input={some string of input}`
parameter. When we see a URL that was output by a client app with that
parameter, we could make clicking that text write that input to the terminal, as
input, rather than using the text as a URL.

We wouldn't need to worry about people sneaking tabs, enters, backspaces into
the input via this - control characters aren't accepted as part of these params.
We would need to figure out how `:` and `;` should be escaped. Those are
delimiters of the sequence itself, so they would need to be escaped / translated
somehow.

We'd need to be wary of things like some of the extended unicode characters - we
don't really want folks clicking links and having commands that do something
like `echo foo ; rm -rf`.

This seems like something that could get roped in with triggers. The [iTerm2
docs] have a *Make Hyperlink* action, which can be used to turn regex-matched
text into a hyperlink. We could always author our equivalent to allow for *Make
Clickable* with a string of input instead.

## Resources

### iTerm2 trigger actions

The following list was taken from the [iTerm2 docs]. I've added notes on ways we could implement similar functionality.

* *Annotate*: Add a bookmark? ✅
* *Bounce Dock Icon*: Visual, window bell? We don't have a manual action for something like this
* *Capture Output*: Not really a good analog. Maybe "add a bookmark", with a bookmark list pane.
* *Highlight Text*: `experimental.colorSelection` above ✅
* *Inject Data*: "Injects a string as though it had been received". This isn't `sendInput`, it's like `sendOutput`. Dangerous for the same reasons that `experimental.colorSelection` is - modifications only to the terminal-side of the conpty buffer.
* *Invoke Script Function*: We have nothing like this. I suppose the general-purpose actions are vaguely like this?
* *Make Hyperlink*: `clickableLink` above ✅
* *Open Password Manager*: Nothing similar.
* *Post Notification*: Consider [#7718] - could absolutely be done.
* *Prompt Detected*: `addMark(prompt)`, basically. ✅
* *Report Directory*: "Tells iTerm2 what your current directory is". Kinda like `sendOutput`, but not that dangerous IMO.
* *Report User & Host*: Kinda the same as the above. We don't use these currently, but we may want to consider in the future.
* *Ring Bell*: Plays the standard system bell sound once.
* *Run Command*: This seems dangerous if misused. Basically just `ShellExecute()` the command.
* *Run Coprocess*: Definitely no precedent we have for this, and might require its own spec.
* *Run Silent Coprocess*: same deal.
* *Send Text*: `sendInput` ✅
* *Set Mark*: `addMark` ✅
* *Set Title*: Similar to the "Report Directory" above.
* *Set User Variable*: Specifically tied to scripting, which we don't have.
* *Show Alert*: Show a toast? [#8592]
* *Stop Processing Triggers*: Definitely an interesting idea. Stop processing more triggers.

Almost all these could be control-level actions that don't _need_ to cross out
of the ControlCore (outside of existing concieved notions for crossing the
boundary, like a visual bell or a notification).

### Footnotes

<a name="footnote-1"><a>[1]: The regex we currently use for URLs is `(\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])`.


[#5916]: https://github.com/microsoft/terminal/issues/5916
[#8849]: https://github.com/microsoft/terminal/issues/8849
[#7718]: https://github.com/microsoft/terminal/issues/7718
[#8592]: https://github.com/microsoft/terminal/issues/8592
[#7562]: https://github.com/microsoft/terminal/issues/7562
[#8294]: https://github.com/microsoft/terminal/issues/8294
[#2671]: https://github.com/microsoft/terminal/issues/2671
[#6969]: https://github.com/microsoft/terminal/issues/6969
[#11901]: https://github.com/microsoft/terminal/issues/11901

[OSC8]: https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
[iTerm2 docs]: https://iterm2.com/documentation-one-page.html#documentation-triggers.html
