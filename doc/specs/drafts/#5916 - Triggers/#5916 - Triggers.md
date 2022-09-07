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

## Solution Design

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

`triggers` is an array of things. First we try to deserialize each thing as a `Control.Trigger`. There's only a few options, and fundamentally those are basically actions`. 
* `addMark`
* `sendInput`
* `experimental.colorSelection`

And we can _probably_ reuse some of the macros in `ActionArgs.h` to make deserializing to a `Control.Trigger` easy. 

`Control.Trigger`'s, when they are hit by the control, will be able to string replace their args themselves. The `git push` trigger above - when matched, the Core will call `Control.SendInputTrigger.Execute(ControlCore core, String[] matches)` (or something like that). `Execute` will be responsible for string-replacing anything it needs to.


If We find that the `action` is NOT a `Control.Trigger`, then we'll make it into an `Settings.Model.Trigger`. We'll hang on to the JSON. When the trigger is hit by the control, it'll raise an event. The app will recieve the event, match it up to the trigger, and call `Model.Trigger.Parse(String[] matches) -> ActionAndArgs` which will do the string replace in the JSON itself. We'll then just take the _whole json_, and try to parse it literally as an `ActionAndArgs`. 



### Turn text into clickable links

A similar request from [#8849] that should also be captured. People want the ability to configure the regexes that are used for turning text into clickable links. Currently, we only match on a predefined set<sup>[[1](#footnote-1)]</sup> into clickable text.

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
* with hyperlink matching, we always assume that the cells themselves contain the payload to turn into a URL. We'd have to instead find that the text matched a trigger, then run it back through the regex to split into matched parts, then parse what the target is. 

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
* [ ] The command palette needs to be able to display both the command name and a comment?
  - This will need to be reconciled with [#7039], which tracks displaying non-localized names in the command palette
* [ ] The command palette is refactored to allow it to interact as the Tasks panel
* [ ] [#1595] An action for opening the tasks panel, filled with all `sendInput` commands
* [ ] Fragments can add **actions** to a user's settings
* [ ] [#10436] Users can manage all their fragments extensions directly in the Settings UI

### 🚶 Walk
* [ ] The terminal can look for a settings file of tasks in a profile's `startingDirectory`
* [ ] [#5790] - profile specific actions
* [ ] [#12927]

### 🏃‍♂️ Run
* [ ] When the user `cd`s to a directory (with shell integration enabled), the terminal can load the tasks from that directory tree
  - Kinda dependent on [#5790] and fragment **actions**, so we understand how they should be layered.
* [ ] Fork of [#12927] - promptable sections can accept a command to dynamically populate options

### 🚀 Sprint
* [ ]
* [ ]


## Conclusion

[comment]: # Of the above proposals, which should we decide on, and why?


### Future Considerations

### Footnotes

<a name="footnote-1"><a>[1]: The regex we currently use for URLs is `(\b(https?|ftp|file)://[-A-Za-z0-9+&@#/%?=~_|$!:,.;]*[A-Za-z0-9+&@#/%=~_|$])`.


[#5916]: https://github.com/microsoft/terminal/issues/5916
[#8849]: https://github.com/microsoft/terminal/issues/8849
