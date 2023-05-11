---
author: Mike Griese @zadjii-msft
created on: 2022-09-07
last updated: 2023-05-11
issue id: 5916
---

# Triggers and Custom Clickable Links

## Abstract

[comment]: # Outline what this spec describes.

## Background

### Inspiration

[comment]: # Are there existing precedents for this type of configuration? These may be in the Terminal, or in other applications

### User Stories

This is a collection of all the possible issues I found on this topic.

* [x] [#5916] Triggers(Including Text Strings) and Actions (internal or external calls)
* [x] [#8849] Allow "hyperlink" matching for arbitrary patterns and adding custom handlers
* [x] [#7562] Allow more URI schemes
* [x] [#2671] Feature Request: link generation for files + other data types
      - This is a dupe of some of the above, honestly. Either [#5916] or [#8849]
* [x] [#6969] Let terminal consumers provide click handlers and pattern recognizers for buffer text
* [x] [#11901] IPv6 links can not be Ctrl+Clicked
* [ ] [#8294] Add a setting to configure display of auto detected links, normally and on hover
* [ ] Probably don't do this for the alt buffer? Make it per-trigger if they work in the alt buffer?

## Solution Design


[TODO!]: # TODO! this was a draft from a comment. Polish.

Example JSON:

```jsonc
"triggers":[
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
        "match": {
            "anchor": "bottom",
            "offset": 4,
            "length": 5,
            "pattern": "    git push --set-upstream origin ([^\\w]*)",
            "runOn": "mark"
        },
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
]
```

`triggers` is an array of `Trigger` objects, which are a combination of an
`ActionAndArgs`, a `Control.Matcher`, and a `Control.TriggerAction`.

```c#
runtimeclass Terminal.Settings.Model.Trigger
{
    Int32 Id;

    Control.Matcher Matcher;
    Control.TriggerAction ControlAction;

    private String _actionJson;
    Terminal.Settings.Model.ActionAndArgs ParseMatches(String[] matches);
}
```

### Trigger matchers

The `match` property is parsed as a `Control.Matcher`. This includes the
following properties, which are heavily inspired by the matchers in VsCode.

```jsonc
"match": {
    "anchor": "bottom",
    "offset": 0, // starting how many rows from the bottom
    "length": 5, // how many lines to match against
    "pattern": "    git push --set-upstream origin ([^\\w]*)",
    "runOn": "everything|newline|mark"
}
```

* `"anchor"` (_default: `"bottom"`_) : Which part of the viewport to run the match against.
  - VsCode has this, but I don't think we really do. I think we can just always say bottom.
* `"offset"` (_default: `0`_) : Run the match starting how many rows from the anchor
* `"length"` (_default: `1`_) : How many rows to include in the match
* `"pattern"` (_required_) : The regex to run. If omitted / empty, this entire
  trigger is ignored.
* `"runOn"` (_default: `"newline"`_) : When to run this match.
  * `"everything"`: On every print from the connection. High performance impact.
  * `"newline"`: On every newline character from the connection. Medium
    performance impact, depending on the workload.
  * `"mark"`: Whenever a mark ([#11000]) is emitted to the terminal. With
    `autoMarkPrompts`, this includes when the user presses <kbd>enter</kbd>.
    This has a much smaller performance impact, assuming the user isn't marking
    every line.

`match` also accepts just a single string, to automatically assume default
values for the other properties. For example, the JSON `"match":
"[Ee][Rr][Rr][Oo][Rr]"` is evaluated as

```jsonc
"match": {
    "anchor": "bottom",
    "offset": 0,
    "length": 1,
    "pattern": "[Ee][Rr][Rr][Oo][Rr]",
    "runOn": "newline"
}
```

VsCode only runs matchers on prompt sequences, for performance reasons. We'll
need to be VERY explicit that anything else is highly detrimental to
performance. We really don't want these to necessarily be running on every
single character the connection emits.

Marks are preferable, because with shell integration, they'll bbe emitted on
every prompt from the shell. Prompts are probably close enough to frequent
enough, and the anchor/offset/length give a subset of the buffer that can be
used to run matches on.

### Trigger actions

Triggers also contain the action that should be performed when a match is found.
These actions are used internally by the control as a `Control.TriggerAction`.
This allows the control to perform the action itself without going out to the
app every time a match is found.

```c#
enum Terminal.Control.TriggerType {
    CustomAction,

    AddMark,
    SendInput,
    ColorSelection,
    ClickableLink,
    ClickableSendInput,
}

runtimeclass Terminal.Control.TriggerAction
{
    TriggerType Type;
    TriggerArgs Args;
}

runtimeclass Terminal.Control.TriggerAndMatch
{
    Control.Matcher Matcher;
    Control.TriggerAction Action;
}

runtimeclass Terminal.Control.CustomActionArgs
{
    UInt32 Index;
    String[] matches;
}
```

There are some built-in `Control.TriggerAction`s (which largely align with
existing `ShortcutAction`s). The supported types of `Control.TriggerAction` the
user can specify are:

* `addMark`: Basically the same as the `addMark` action
* `sendInput`: Basically the same as the `sendInput` action
* `experimental.colorSelection`: Basically the same as the `sendInput` action
* `clickableLink`: Turn the matched text into a clickable link. When the user
  clicks that text, we'll attempt to `ShellExecute` that link, much like a URL.
* `clickableSendInput`: Similarly, turn the matched text into a clickable region
  which, when clicked, sends that text to the connection as input.

However, if the JSON of the trigger doesn't evaluate as a
`Control.TriggerAction`, we'll instead try to parse the json as a
`ActionAndArgs`. If we find that, then we'll use a special
`Control.TriggerAction` type:

* `customAction`: This is an internal `TriggerAction` type. This is used to
  allow the app to provide its own action handler. The control will raise a
  `CustomAction` event, with args containing the index of the trigger that
  matched, and the matching groups of the regex.

`Control.Trigger`'s, when they are hit by the control, will be able to string
replace their args themselves. For example:
* The `git push` trigger above - when matched, the Core will call
  `Control.SendInputTrigger.Execute(ControlCore core, String[] matches)` (or
  something like that). `Execute` will be responsible for string-replacing
  anything it needs to.

For custom actions (READ: settings model `ShortcutAction`s, like `newTab`),
we'll hang on to the JSON. When the trigger is hit by the control, it'll raise
an event. The app will recieve the event, match it up to the trigger, and call
`Model.Trigger.Parse(String[] matches) -> ActionAndArgs`. That will do the
string replace in the JSON itself (similar to how we do iterable `Command`
expansion). We'll then just take the _whole json_, and try to parse it literally
as an `ActionAndArgs`. This does mean that we couldn't use `match` as a property
for any other future actions.

We can reuse some of the macros in `ActionArgs.h` to make
deserializing to a `Control.Trigger` easy.

#### Alternative JSON for consideration

This one encapslates the actions into the `command` property of the
`Terminal.Settings.Model.Trigger`. In this version, we don't need to try and
reparse the entire JSON object as a `ActionAndArgs`. This will let future
actions use the `match` property.

```jsonc
"triggers":[
    {
        "match": "[Ee][Rr][Rr][Oo][Rr]",
        "command": {
            "action": "addMark",
            "color": "#ff0000"
        },
    },
    {
        "match": "Terminate batch job \\(Y/N\\)",
        "command": {
            "action": "sendInput",
            "input": "y\r\n"
        },
    },
    {
        "match": {
            "anchor": "bottom",
            "offset": 4,
            "length": 5,
            "pattern": "    git push --set-upstream origin ([^\\w]*)",
            "runOn": "mark"
        },
        "command": {
            "action": "sendInput",
            "input": "git push --set-upstream origin ${match[1]}\r"
        },
    },
    {
        "match": "'(.*)' is not recognized as an internal or external command,",
        "command": {
            "action": "checkWinGet",
            "commandline": "${match[1]}"
        },
    },
    {
        "match": "This will open a new Terminal tab",
        "command": "newTab"
    },
]
```


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

#### Clickable `sendInput`

This is the same idea as above. We want users to be able to magicially turn regions of the buffer into interactive content. When clicking on that content, the text will get written to the input, rather than `ShellExecute`d.

```jsonc
{
    "match": "    git push --set-upstream origin ([^\\w]*)",
    "action": "clickableSendInput",
    "input": "git push --set-upstream origin ${match[1]}"
},
````


### Disabling triggers

Three main cases:
* If we define a trigger in `defaults.json`, users should be able to disable it.
* If a user puts a trigger into `profiles.defaults`, but doesn't want it to show up in a specific profile
* If a fragment decides to add a trigger to a specific profile

[TODO!]: # TODO!

We'll just use `"id"` as a key in a map of triggers. Users can unbind them with
`"id": "Terminal.builtInThing", match:""`. Just like global actions.

### Triggers for Terminal Control consumers

Case in point: [#6969]. Visual Studio would like to supply their own trigger,
and recieve an event with the ID of the trigger.

> * Consumers of the terminal must be able to provide a pattern that will identify regions of clickable text
> * Consumers will provide a callback that is called with the clicked text as a parameter

These are addressed generally by the rest of the spec. They could provide
`Control.TriggerAndMatch` via `ICoreSettings`. They can use the built-in control
actions, or similar to the Windows Terminal, handle custom actions on the
control's `CustomAction` event handler.

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

The following list was taken from the [iTerm2 docs]. I've added notes on ways we
could implement similar functionality. Where there's room for us to easily add
new Control-level actions, I've added the enum name as a sub-point.

* *Annotate*: Add a bookmark? ✅
* *Bounce Dock Icon*: Visual, window bell? We don't have a manual action for something like this
    * `Control.TriggerType.RaiseBell`
* *Capture Output*: Not really a good analog. Maybe "add a bookmark", with a bookmark list pane.
* *Highlight Text*: `experimental.colorSelection` above ✅
* *Inject Data*: "Injects a string as though it had been received". This isn't `sendInput`, it's like `sendOutput`. Dangerous for the same reasons that `experimental.colorSelection` is - modifications only to the terminal-side of the conpty buffer.
* *Invoke Script Function*: We have nothing like this. I suppose the general-purpose actions are vaguely like this?
* *Make Hyperlink*: `clickableLink` above ✅
* *Open Password Manager*: Nothing similar.
* *Post Notification*: Consider [#7718] - could absolutely be done.
    * `Control.TriggerType.SendNotification`
* *Prompt Detected*: `addMark(prompt)`, basically. ✅
* *Report Directory*: "Tells iTerm2 what your current directory is". Kinda like `sendOutput`, but not that dangerous IMO.
    * `Control.TriggerType.SetWorkingDirectory`
* *Report User & Host*: Kinda the same as the above. We don't use these currently, but we may want to consider in the future.
* *Ring Bell*: Plays the standard system bell sound once.
    * `Control.TriggerType.RaiseBell`
* *Run Command*: This seems dangerous if misused. Basically just `ShellExecute()` the command.
* *Run Coprocess*: Definitely no precedent we have for this, and might require its own spec.
* *Run Silent Coprocess*: same deal.
* *Send Text*: `sendInput` ✅
* *Set Mark*: `addMark` ✅
* *Set Title*: Similar to the "Report Directory" above.
    * `Control.TriggerType.SetTitle`
* *Set User Variable*: Specifically tied to scripting, which we don't have.
* *Show Alert*: Show a toast? [#8592]
    * `Control.TriggerType.SendInAppToast`
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
