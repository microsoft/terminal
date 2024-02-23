---
author: Mike Griese
created on: 2023-01-26
last updated: 2023-01-27
issue id: n/a
---
# Windows Terminal Copilot | Prompting

## Abstract

GitHub Copilot is a fairly revolutionary tool that offers complex predictions
for code from the context of the file you're working on and some simple
comments. We envision a scenario where this AI model can be integrated directly
within the Terminal application. This would enable users to type a natural
language description of what they're hoping to do, and recieve suggested
commands to accomplish that task. This has the potential to remove the need for
commandline users to memorize long sets of esoteric flags and options for
commands. Instead, they can simply describe what they want done, and _do it_.

This is one of the many scenarios being considered under the umbrella of "AI in the Terminal". For the other scenarios, see [Overview].

## Background

### Inspiration

Github's own Copilot service was what sparked the initial interest in this area.
This quickly lead to the thought "If it can do this for code, can it work for
command lines too?".

This likely started a cascade of similar implementations across the command-line
ecosystem. Both [Fig] and [Warp] have produced similar compelling user
experiences already, powered by AI. Github Labs are also working on a similar
natural language-to-command model with [Copilot CLI].

This seems to be one of the scenarios that can generate the most value quickly
with existing AI models, which is why it's generated so much interest.

### User Stories

The following plan was made with consideration for what's possible _before Build 2023_, alongside what we want to do _in the fullness of time_.

#### By Build

Story |  Size | Description
--|-----------|--
A | ✅ Done  | The user can "disable" the extension (by unbinding the action)
A | 🐣 Crawl  | The user can use an action to open a dedicated "AI Palette" for prompt-driven AI suggestions.
A | 🐣 Crawl  | Suggested results appear as text in the Terminal Control, before the user accepts the command
A | 🐣 Crawl  | The AI palette can use a manual API key in the settings to enable openAI access
A | 🚶 Walk  | The AI Palette uses an official authentication method (Github login, DevID, etc.)
A | 🚶 Walk  | The AI Palette remembers previous queries, for quick recollection and modification.
A | 🚶 Walk  | The AI Palette informs the user if they're not authenticated to use the extension

#### After Build

Story |  Size | Description
--|-----------|--
A | 🚶 Walk   | The AI palette is delivered as an extension to the Terminal
A | 🏃‍♂️ Run    | The AI Palette can be moved, resized while hovering
A | 🏃‍♂️ Run    | The AI Palette can be docked from a hovering control to a Pane

### Elevator Pitch

It's Copilot. For the command line.

## Business Justification

It will delight developers.

## Scenario Details

### UI/UX Design

![A VERY rough mockup of what this UI might look like](./img/Copilot-in-cmdpal.png)

> **Warning**: TODO! Get mocks from Rodney

### Implementation Details

We'll add a new Control to the Terminal, which we'll dub the `AiPalette`. This
will be forked from the `CommandPalette` code initially, but not built directly
in to it. This `AiPalette` will have a text box, and should be capable of "previewing" actions, in the same way that the Command Palette is. The only action it should need to preview is `sendInput` (which has a prototype implementation linked to [#12861]).

We'll add a new action to invoke this `AiPalette`, which we'll temporarily call
`experimental.ai.prompt`. This will work vaguely like the `commandPalette`
action.

Considering the UX pattern for the OpenAI models is largely conversational, it
will be helpful to users to have a history of the requests they've made, and the
results the model returned, in the UI somewhere. We can store these previous
commands and results in an array in `state.json`. This would work similar to the
way the Command Palette's Commandline mode works currently. We'll need to make a
small modification to store and array of `{prompt, result}` objects, but that
should be fairly trivial.

#### Authentication
<sup>_By Build 2023_</sup>

We don't know if this will be powered by Github Copilot, or some other
authentication method.

While we sort that out, we'll need to make engineering progress, regardless. To
facilitate that, we should just add temporary support for a user to paste an
OpenAI API key in the `settings.json`. This should be good enough to get us
unblocked and making progress with at least one AI model, which we sort out the
specifics of authentication and the backend.

> **Warning**: TODO! Figure out what the official plan here will be, and do that.

#### `HoverPane`
<sup>_By Build 2023_</sup>

After the initial implementation of the `AiPalette`, we'll want to refactor the code slightly to enable arbitrary content to float above the Terminal. This would provide a consistent UI experience for transient content.

This would be something like a `HoverPane` control, which accepts a
`FrameworkElement` as the `Content` property. We'd extract out the actual list
view, text box, etc. of the `AiPalette`  and instead invoke a new `HoverPane`
with that `AiPalette` as the content.

This we want to do _before_ Build. This same `HoverPane` could be used to
support **[Explain that]**. That's another scenario we'd like demo'd by Build,
so being able to re-use the same UI base would make sense.

This would also make it easy to swap out the `Content` of the `HoverPane` to
replace it with whatever we need to support authentication flows.

> **Warning**: TODO! Refine this idea after we get mocks from design.

#### Pinning a `HoverPane` to a first-class `Pane`
<sup>_After Build 2023_</sup>

This will require us to support non-terminal content in `Pane`s ([#977]). `Pane`
as a class if very special cased for hosting a `TermControl`, and hosting other
types of `FrameworkElement`s is something that will take some refactoring to
enable. For more details, refer to the separate spec detailing [non-terminal panes](https://github.com/microsoft/terminal/blob/main/doc/specs/drafts/%23997%20Non-Terminal-Panes.md).

Pinning the `HoverPane` would create a new pane, split from the currently active pane.

> **Warning**: TODO! Refine this idea after we get mocks from design.

#### Moving and resizing the `HoverPane`
<sup>_After Build 2023_</sup>

> **Warning**: TODO! after build.

#### Send feedback on the quality of the suggestions
<sup>_After Build 2023_</sup>

> **Warning**: TODO! after build.

## Tenents

<table>

<tr><td><strong>Compatibility</strong></td><td>

We don't expect any regressions while implementing these new features.

</td></tr>

<tr><td><strong>Accessibility</strong></td><td>

Largely, we expect the `AiPalette` to follow the same UIA patterns laid out by the Command Palette before it.

</td></tr>

<tr><td><strong>Localization</strong></td><td>

This feature might end up making the Terminal _more_ accessible to users who's
primary language is not English. The commandline is a fairly ascii-centric
experience in general. It might be a huge game changer for users from
less-represented languages to be able to describe in their native language what
they want to do. They wouldn't need to parse search results from the web that
might not be in their native language. The AI model would do that for them.

</td></tr>

</table>

[comment]: # If there are any other potential issues, make sure to include them here.

## To-do list

## Before Build Todo's

### 🐣 Crawl
* [ ] Introduce a new `AiPalette` control, initially forked from the
  `CommandPalette` code
  * [ ] TODO! We need design comps to really know what to build here.
  * [ ] For the initial commit, just have it accept a prompt and generate a fake
    / placeholder "response"
* [ ] Add a placeholder `experimental.ai.prompt` `ShortcutAction` to open that
  `AiPalette`. Bind to no key by default.
* [ ] Make `sendInput` actions previewable, so the text will appear in the
  `TermControl` as a _preview_.
* [ ] Hook up an AI model to it. Doesn't have to be the real, final one. Just
  _an_ AI model.
* [ ]

### 🚶 Walk
* [ ] Stash the queries (and responses?) in `state.json`, so that we can bring
  them back immediately (like the Commandline Mode of the CommandPalette)
* [ ] Move the content of the `AiPalette` into one control, that's hosted by a
  `HoverPane` control
  * this would be to allow **[Explain that]** to reuse the `HoverPane`.
  * This can easily be moved to post-Build if we don't intend to demo [Explain
    that] at Build.
* [ ] If the user isn't authenticated when running the `experimental.ai.prompt`
  action, open the `HoverPane` with a message telling them how to (or a control
  enabling them to)
* [ ] If the user **is** authenticated when running the `experimental.ai.prompt`
  action, **BUT** bot authorized to use that model/billing/whatever, open the
  `HoverPane` with a message explaining that / telling them how to.
  * Thought process: Copilot is another fee on top of your GH subscription. You
    might be able to log in with your GH account, but not be allowed to use
    copilot.
* [ ]

## After Build Todo's

### 🚶 Walk

* [ ] Extensions can add custom `ShortcutAction`s to the Terminal
  * [ ] Change the string for this action to something more final than `experimental.ai.prompt`
* [ ] Extensions can add UI elements to the Terminal window
* [ ] Extensions can request the Terminal open a `HoverPane` and specify the
  content for that pane.
* [ ] Extensions can add `Page`s to the Terminal settins UI for their own settings
* [ ] The `AiPalette` control is moved out of the Terminal proper and into a
  separate app package
* [ ] ...

### 🏃‍♂️ Run

> The AI Palette can be moved, resized while hovering
> The AI Palette can be docked from a hovering control to a Pane

* [ ] Enable the `HoverPane` control to be resizable with the mouse
* [ ] Enable the `HoverPane` control to be dragable with the mouse
  * i.e., instead of being strictly docked to the left of the screen, it's got a
    little grabby icon / titlebar that can be used to reposition it.
* [ ] Enable `Pane`s to host non-terminal content
* [ ] Add a button to`HoverPane` to cause it to be docked to the currently active pane
  * this will open a new `auto` direction split, taking up whatever percent of
    the parent is necessary to achieve the same size as the `HoverPane` had
    before(?)
* [ ] ...

### 🚀 Sprint
* [ ] ...

## Conclusion

### Rejected ideas

**Option 1**: Use the [Suggestions UI] for this.
* **Pros**:
  * the UI appears right at the place the user is typing, keeing them exactly in
    the context they started in.
  * Suggestion `source`s would be easy/cheap to add as an extension, with
    relatively few Terminal changes (especially compared with adding
    extension-specific actions)
* **Cons**:
  * The model of prompting, then navigating results that are delivered
    asynchronously, is fundamentally not compatible with the way the suggestions
    UI works.


**Option 2**: Create a new Command Palette Mode for this. This was explored in greater detail
over in the [Extensions] doc.
* **Pros**: "cheap", we can just reuse the Command Palette for this. _Perfect_, right?
* **Cons**:
  * Probably more expensive than it's worth to combine the functionality with
    the Command Palette. Best to just start fresh with a new control that
    doesn't need to carry the baggage of the other Command Palette modes.
  * When this does end up being delivered as a separate package (extension), the
    fullness of what we want to customize about this UX would be best served by
    another UI element anyways. It'll be VERY expensive to instead expose knobs
    for extensions to fully customize the existing palette.

### Future Considerations

The flexibility of the `HoverPane` to display arbitrary content could be
exceptionally useful in the future. All sorts of UI elements that we've had no
place to put before could be placed into `HoverPane`s. [#644], [#1595], and
[#8647] are all extension scenarios that would be able to leverage this.

## Resources

### Footnotes

<a name="footnote-1"><a>[1]:


[Fig]: https://fig.io/user-manual/ai
[Warp]: https://docs.warp.dev/features/entry/ai-command-search
[Copilot CLI]: https://githubnext.com/projects/copilot-cli/

[Terminal North Star]: ../Terminal-North-Star.md
[Tasks]: ../Tasks.md
[Shell Integration]: ../Shell-Integration-Marks.md
[Suggestions UI]: ../Suggestions-UI.md
[Extensions]: ../Suggestions-UI.md

[Overview]: ./Overview.md
[Implicit Suggestions]: ./Implicit-Suggestions.md
[Prompting]: ./Prompting.md
[Explain that]: ./Explain-that.md

<!-- TODO! -->
[shell-driven autocompletion]: ../Terminal-North-Star.md#Shell_autocompletion


[#977]: https://github.com/microsoft/terminal/issues/997
[#12861]: https://github.com/microsoft/terminal/issues/12861

[#4000]: https://github.com/microsoft/terminal/issues/4000
[#644]: https://github.com/microsoft/terminal/issues/644
[#1595]: https://github.com/microsoft/terminal/issues/1595
[#8647]: https://github.com/microsoft/terminal/issues/8647
