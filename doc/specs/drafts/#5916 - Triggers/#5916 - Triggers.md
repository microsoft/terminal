---
author: Mike Griese @zadjii-msft
created on: 2022-09-07
last updated: 2023-07-12
issue id: 5916
---

# Triggers and Custom Clickable Links

## Abstract

This spec outlines a mechanism by which users can define custom actions to run
when a string of text is written to the Terminal. This lets users create
powerful ways of automating their Terminal to match their own workflows. This
same mechanism can be used by third-party applications to customize the way the
terminal control automatically identifies links or other clickable regions of
the buffer, and handle it in their own way.

## Background

### Inspiration

Much of this was heavily inspired by the [VsCode "auto replies"], as well as the
[triggers in iTerm2]. VsCode is also working on a draft "[Quick Fix API]", from
which a lot of inspiration was taken as far as crafting the `match` syntax. You
can seen a sample of their syntax
[here](https://github.com/microsoft/vscode/blob/4c6e0b5eee419550e31e386a0a5ca9840ac3280b/extensions/npm/package.json#L342-L354).

### User Stories

This is a collection of all the possible issues I found on this topic.

* [x] [#5916] Triggers(Including Text Strings) and Actions (internal or external calls)
* [x] [#8849] Allow "hyperlink" matching for arbitrary patterns and adding custom handlers
* [x] [#7562] Allow more URI schemes
* [x] [#2671] Feature Request: link generation for files + other data types
      - This is a dupe of some of the above, honestly. Either [#5916] or [#8849]
* [x] [#6969] Let terminal consumers provide click handlers and pattern recognizers for buffer text
* [x] [#11901] IPv6 links can not be Ctrl+Clicked
* [x] Probably don't do this for the alt buffer? Make it per-trigger if they work in the alt buffer?

Not addressed as a part of this spec:

* [ ] [#8294] Add a setting to configure display of auto detected links, normally and on hover

We'll probably want to come back and separately spec how users can control the
appearance of both auto-detected clickable text, and links that are manually
emitted to the Terminal (via OSC 8). This may be increasingly relevant, as this
spec will introduce new ways for users to make text clickable.

## UX / UI Design

These are some prototypes from initial investigations in the space. The settings
in these are not up to the current spec.

![gh-5916-prototype-000](https://user-images.githubusercontent.com/18356694/185226626-c723be0d-501c-40f1-9b2f-f017932ea4fe.gif)

_fig 1: a `sendInput` trigger that sends "y" any time that "Terminate batch job" is written to the Terminal_


![image](https://user-images.githubusercontent.com/18356694/185231958-46c73bcc-4ef4-4dfe-a631-593649a64b75.png)

_fig 2: A trigger that adds a red error mark whenever an app writes "error" to the Terminal_


![gh-5916-prototype-001](https://user-images.githubusercontent.com/18356694/185247128-b3b5c88a-e46c-4a9c-8a03-fd45b00ebc70.gif)

_fig 3: A sendInput trigger which sends some of the matched text_


## Solution Design

To support "triggers", we'll add a new object to the profile settings called
`triggers`. This is an array of objects that describe

1. What string to match on for running the trigger
2. What to do when that match is found.

Let's start with an example blob of JSON:

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

> **Note**
> Interface and other types here are shown in C#, since that's fairly close to
> MIDL3, which would be used for cross-component interfaces.

```c#
runtimeclass Terminal.Settings.Model.Trigger
{
    String Id;

    Control.Matcher Matcher;
    Control.TriggerAction ControlAction;

    Terminal.Settings.Model.ActionAndArgs ParseMatches(String[] matches);

    private String _actionJson;
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
    "runOn": "everything|newline|mark",
    "buffer": "main|alt|any"
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
* `"buffer"` (_default_: `"any"`): Configure which terminal buffer this trigger runs on.
  * `main`: only run in the main buffer (e.g., most shells)
  * `alt`: only run in the alternate screen buffer (e.g., `vim`, `tmux`, any full-screen application)
  * `any`: Run in either buffer.

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

Marks are preferable, because with shell integration, they'll be emitted on
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
    internal void Execute(ControlCore core, String[] matches);
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
an event. The app will receive the event, match it up to the trigger, and call
`Model.Trigger.Parse(String[] matches) -> ActionAndArgs`. That will do the
string replace in the JSON itself (similar to how we do iterable `Command`
expansion). We'll then just take the _whole json_, and try to parse it literally
as an `ActionAndArgs`. This does mean that we couldn't use `match` as a property
for any other future actions.

We can reuse some of the macros in `ActionArgs.h` to make
deserializing to a `Control.Trigger` easy.

#### Alternative JSON for consideration

This one encapsulates the actions into the `command` property of the
`Terminal.Settings.Model.Trigger`. In this version, we don't need to try and
re-parse the entire JSON object as a `ActionAndArgs`. This will let future
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

This is the same idea as above. We want users to be able to magically turn
regions of the buffer into interactive content. When clicking on that content,
the text will get written to the input, rather than `ShellExecute`d.

```jsonc
{
    "match": "    git push --set-upstream origin ([^\\w]*)",
    "action": "clickableSendInput",
    "input": "git push --set-upstream origin ${match[1]}"
},
````

### Layering & Disabling triggers

Three main cases:
* If we define a trigger in `defaults.json`, users should be able to disable it.
* If a user puts a trigger into `profiles.defaults`, but doesn't want it to show
  up in a specific profile
* If a fragment decides to add a trigger to a specific profile

We actually _can't_ put triggers into `defaults.json`. For complicated reasons,
we can't have a `profiles.defaults` block in `defaults.json`. This has caused
issues in the past. So there's no way for the Terminal to pre-define triggers
for all profiles in `defaults.json`. We could entertain the idea of a
`defaults.json`-only global `triggers` property, that would work as the
"default" triggers. I don't want to commit to that until we have more use cases
for default triggers than just the URL detection one.

We'll just use `"id"` as a key in a map of triggers. Users can unbind them with
`"id": "Terminal.builtInThing", match:""`. Just like global actions. So a user can have:

```json
"profiles": {
    "defaults": {

        "triggers": [
            {
                "id": "my winget thing",
                "match": "'(.*)' is not recognized as an internal or external command,",
                "command": {
                    "action": "checkWinGet",
                    "commandline": "${match[1]}"
                },
            },

        ]
    },
    "list": [
        {
            "name": "my profile",
            "commandline": "wsl.exe",
            "triggers": [
                { "id": "my winget thing", "match":"" }

            ]
        }
    ]
}
```

Triggers do not support partial layering. In the previous example, doing something like:

```json
{ "id": "my winget thing", "command": "newTab" }
```

would not have created a trigger with the same regex match to perform a `newTab`
action instead. That json in the profile would be treated as not having a
`match`, and would then "unbind" the action from the defaults.

### Triggers for Terminal Control consumers

Case in point: [#6969]. Visual Studio would like to supply their own trigger,
and receive an event with the ID of the trigger.

> * Consumers of the terminal must be able to provide a pattern that will identify regions of clickable text
> * Consumers will provide a callback that is called with the clicked text as a parameter

These are addressed generally by the rest of the spec. They could provide
`Control.TriggerAndMatch` via `ICoreSettings`. They can use the built-in control
actions, or similar to the Windows Terminal, handle custom actions on the
control's `CustomAction` event handler.

## Potential Issues

<table>

<tr><td><strong>Compatibility</strong></td><td>

How does someone turn off the built-in hyperlink detector? Currently, the
Terminal has a global setting `experimental.detectURLs` to enable auto-detection
of URLs. That should theoretically be replaced with an equivalent setting:

```json
"triggers": [
    {
        "match": "(\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])",
        "action": "clickableLink",
        "target": "https://example.com/tasks/${match[1]}"
    },
]
```

However, as mentioned before, this _can't_ go in
`defaults.json@profiles.defaults`. We could:

* Promote the global `experimental.detectURLs` to a fully fledged setting
  `detectURLs`. This would automatically insert this trigger into the list of
  triggers whenever it is enabled. We could make it a per-profile setting. We'll
  just always add it at the end of their list of triggers when creating settings
  for the profile.
* Alternatively, implicitly have a "hardcoded default" trigger with the id of
  `"Microsoft.Terminal.DetectUrls"`. That would let folks opt-out with
  ```json
  { "id": "Microsoft.Terminal.DetectUrls", "match":"" }
  ```


</td></tr>

<tr><td><strong>Accessibility</strong></td><td>

Clickable links and sendInput will get the same appearance treatment as
auto-detected links (but not as manually emitted ones). They'll have the same
accessibility properties as clickable text does today.

Otherwise, no accessibility changes expected.

</td></tr>

<tr><td><strong>Sustainability</strong></td><td>

No sustainability changes expected.

</td></tr>

<tr><td><strong>Localization</strong></td><td>

No localization concerns here.

</td></tr>

</table>

* Q: What happens if multiple regexes match the same text
  - A: We'll apply the regexes in the order they're defined in the JSON.


### Future Considerations

* The "iTerm2 trigger actions" listed below are a great list of potential
  follow-up ways these could be used.

<!-- This was originally in this doc, but I think that should be moved to it's own spec -->
<!--
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
Clickable* with a string of input instead. -->

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
* *Report Directory*: "Tells iTerm2 what your current directory is". Kinda like `sendOutput`, but not that dangerous in my opinion.
    * `Control.TriggerType.SetWorkingDirectory`
* *Report User & Host*: Kinda the same as the above. We don't use these currently, but we may want to consider in the future.
* *Ring Bell*: Plays the standard system bell sound once.
    * `Control.TriggerType.RaiseBell`
* *Run Command*: This seems dangerous if misused. Basically just `ShellExecute()` the command.
* *Run Co-process*: Definitely no precedent we have for this, and might require its own spec.
* *Run Silent Co-process*: same deal.
* *Send Text*: `sendInput` ✅
* *Set Mark*: `addMark` ✅
* *Set Title*: Similar to the "Report Directory" above.
    * `Control.TriggerType.SetTitle`
* *Set User Variable*: Specifically tied to scripting, which we don't have.
* *Show Alert*: Show a toast? [#8592]
    * `Control.TriggerType.SendInAppToast`
* *Stop Processing Triggers*: Definitely an interesting idea. Stop processing more triggers.

Almost all these could be control-level actions that don't _need_ to cross out
of the ControlCore (outside of existing conceived notions for crossing the
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
[#11000]: https://github.com/microsoft/terminal/issues/11000

[OSC8]: https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda
[triggers in iTerm2]: https://iterm2.com/documentation-one-page.html#documentation-triggers.html
[iTerm2 docs]: https://iterm2.com/documentation-one-page.html#documentation-triggers.html
[VsCode "auto replies"]: https://code.visualstudio.com/docs/terminal/advanced#_auto-replies
[Quick Fix API]: https://github.com/microsoft/vscode/blob/4c6e0b5eee419550e31e386a0a5ca9840ac3280b/src/vs/workbench/contrib/terminalContrib/quickFix/browser/terminalQuickFixService.ts#L80-L146
