---
author: Mike Griese
created on: 2022-03-28
last updated: 2023-07-19
issue id: 11000, 1527, 6232
---

##### [Original issue: [#1527]] [experimental PR [#12948]] [remaining marks [#14341]]

# Windows Terminal - Shell Integration (Marks)

## Abstract

_"Shell integration" refers to a broad category of ways by which a commandline
shell can drive richer integration with the terminal. This spec in particular is
most concerned with "marks" and other semantic markup of the buffer._

Marks are a new buffer-side feature that allow the commandline application or
user to add a bit of metadata to a range of text. This can be used for marking a
region of text as a prompt, marking a command as succeeded or failed, quickly
marking errors in the output. These marks can then be exposed to the user as
pips on the scrollbar, or as icons in the margins. Additionally, the user can
quickly scroll between different marks, to allow easy navigation between
important information in the buffer.

Marks in the Windows Terminal are a combination of functionality from a variety
of different terminal emulators. "Marks" attempts to unify these different, but
related pieces of functionality.

## Background

There's a large amount of prior art on this subject. I've attempted to collect
as much as possible in the ["Relevant external docs"](#Relevant-external-docs)
section below. "Marks" have been used in different scenarios by different
emulators for different purposes. The common thread running between them of
marking a region of text in the buffer with a special meaning.

* iTerm2, ConEmu, FinalTerm et.al. support emitting a VT sequence to indicate
  that a line is a "prompt" line. This is often used for quick navigation
  between these prompts.
* FinalTerm (and xterm.js) also support marking up more than just the prompt.
  They go so far as to differentiate the start/end of the prompt, the start of
  the commandline input, and the start/end of the command output.
  `FTCS_COMMAND_FINISHED` is even designed to include metadata indicating
  whether a command succeeded or failed.
* Additionally, Terminal.app allows users to "bookmark" lines via the UI. That
  allows users to quickly come back to something they feel is important.
* Consider also editors like Visual Studio. VS also uses little marks on the
  scrollbar to indicate where other matches are for whatever the given search
  term is.

### "Elevator" pitch

The Terminal provides a way for command line shells to semantically mark parts
of the command-line output. By marking up parts of the output, the Terminal can
provide richer experiences. The Terminal will know where each command starts and
stops, what the actual command was and what the output of that command is. This
allows the terminal to expose quick actions for:

* Quickly navigating the history by scrolling between commands
* Re-running a previous command in the history
* Copying all the output of a single command
* A visual indicator to separate out one command-line from the next, for quicker
  mental parsing of the output of the command-line.
* Collapsing the output of a command, as to reduce noise
* Visual indicators that highlight commands that succeeded or failed.
* Jumping to previously used directories

### User Stories

This is a bit of a unusual section, as this feature was already partially
implemented when this spec was written.

Story |  Size | Description
--|-----------|--
A | ✅ Done   | The user can use mark each prompt and have a mark displayed on the scrollbar
B | ✅ Done   | The user can perform an action to scroll between marks
C | ✅ Done   | The user can manually add marks to the buffer
D | ✅ Done   | The shell can emit different marks to differentiate between prompt, command, and output
E | ✅ Done   | Clearing the buffer clears marks
F | 🐣 Crawl  | Marks stay in the same place you'd expect after resizing the buffer.
G | ✅ Done   | Users can perform an action to select the previous command's output
H | 🚶 Walk   | The find dialog can display marks on the scrollbar indicating the position of search matches
I | 🏃‍♂️ Run    | The terminal can display icons in the gutter, with quick actions for that command (re-run, copy output, etc)
J | 🏃‍♂️ Run    | The terminal can display a faint horizontal separator between commands in the buffer.
K | 🚀 Sprint | The terminal can "collapse" content between two marks.
L | 🚀 Sprint | The terminal can display a sticky header above the control which displays the most recent command
M | 🚀 Sprint | The user can open a dialog to manually bookmark a line with a custom comment

## Solution Design

### Supported VT sequences

* [x] iTerm2's OSC `SetMark` (in [#12948])
* [x] FinalTerm prompt markup sequences
  - [x] **FTCS_PROMPT** was added in [#13163]
  - [x] The rest in [#14341]
* [ ] additionally, VsCode's FinalTerm prompt markup variant, `OSC 663`
* [ ] [ConEmu's
  `OSC9;12`](https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC)
* [ ] Any custom OSC we may want to author ourselves.

The FinalTerm prompt sequences are probably the most complicated version of all
these, so it's important to give these a special callout. Almost all the other
VT sequences are roughly equivalent to **FTCS_PROMPT**. The xterm.js / VsCode
version has additional cases, that they ironically added to work around conpty
not understanding these sequences originally.

#### FinalTerm sequences

The relevant FinalTerm sequences for marking up the prompt are as follows.


![image](FTCS-diagram.png)

* **FTCS_PROMPT**: `OSC 133 ; A ST`
  - The start of a prompt. Internally, this sets a marker in the buffer
    indicating we started a prompt at the current cursor position, and that
    marker will be used when we get a **FTCS_COMMAND_START**
* **FTCS_COMMAND_START**: `OSC 133 ; B ST`
  - The start of a commandline (READ: the end of the prompt). When it follows a
    **FTCS_PROMPT**, it creates a mark in the buffer from the location of the
    **FTCS_PROMPT** to the current cursor position, with the category of
    `prompt`
* **FTCS_COMMAND_EXECUTED**: `OSC 133 ; C ST`
  - The start of the command output / the end of the commandline.
* **FTCS_COMMAND_FINISHED**: `OSC 133 ; D ; [Ps] ST`
  - the end of a command.

Same deal for the **FTCS_COMMAND_EXECUTED**/**FTCS_COMMAND_FINISHED** ones.
**FTCS_COMMAND_EXECUTED** does nothing until we get a **FTCS_COMMAND_FINISHED**,
and the `[Ps]` parameter determines the category.
  - `[Ps] == 0`: success
  - anything else: error

This whole sequence will get turned into a single mark.

When we get the **FTCS_COMMAND_FINISHED**, set the category of the prompt mark
that preceded it, so that the `prompt` becomes an `error` or a `success`.


### Buffer implementation

In the initial PR ([#12948]), marks were stored simply as a `vector<Mark>`,
where a mark had a start and end point. These wouldn't reflow on resize, and
didn't support all of the FTCS sequences.

There's ultimately three types of region here we need to mark:
* The prompt (starting from A)
* the command (starting from B)
* the output (starting from C)

That intuitively feels a bit like a text attribute almost. Additionally, the
prompt should be connected to its subsequent command and output, s.t. we can
* Select command output
* re-run command

easily. Supposedly, we could do this by iterating through the whole buffer to
find the previous/next {whatever}[[1](#footnote-1)]. Additionally, the prompt
needs to be able to contain the status / category, and a `133;D` needs to be
able to change the category of the previous prompt/command.

If we instead do a single mark for each command, from `133;A` to `133;A`, and
have sub-points for elements within the command
* `133;A` starts a mark on the current line, at the current position.
* `133;B` sets the end of the mark to the current position.
* `133;C` updates the mark's `commandStart` to the current end, then sets the
  end of the mark to the current position.
* `133;D` updates the mark's `outputStart` to the current end, then sets the end
  of the mark to the current position. It also updates the category of the mark,
  if needed.

Each command then only shows up as a single mark on the scrollbar. Jumping
between commands is easy, `scrollToMark` operates on `mark.start`, which is
where the prompt started. "Bookmarks", i.e. things started by the user wouldn't
have `commandStart` or `outputStart` in them. Getting the text of the command,
of the output is easy - it's just the text between sub-points.

Reflow still sucks though - we'd need to basically iterate over all the marks as
we're reflowing, to make sure we put them into the right place in the new
buffer. This is annoying and tedious, but shouldn't realistically be a
performance problem.

#### Cmd.exe considerations

`cmd.exe` is generally a pretty bad shell, and doesn't have a lot of the same
hooks that other shells do, that might allow for us to emit the
**FTCS_COMMAND_EXECUTED** sequence. However, cmd.exe also doesn't allow
multiline prompts, so we can be relatively certain that when the user presses
<kbd>enter</kbd>, that's the end of the prompt. We will treat the
`autoMarkPrompts` setting (which auto-marks <kbd>enter</kbd>) as the _end of the
prompt_. That would at least allow cmd.exe to emit a {command finished}{prompt
start}{prompt...}{command start} in the prompt, and have us add the command
executed. It is not perfect (we wouldn't be able to get error information), but
it does work well enough.

```cmd
PROMPT $e]133;D$e\$e]133;A$e\$e]9;9;$P$e\%PROMPT%$e]133;B$e\
```

## Settings proposals

The below are the proposed additions to the settings for supporting marks and
interacting with them. Some of these have already been added as experimental
settings - these would be promoted to no longer be experimental.

Many of the sub-points on these settings are definitely "Future Consideration"
level settings. For example, the `scrollToMark` `"highlight"` property. That one
is certainly not something we need to ship with.

### Actions

In addition to driving marks via the output, we will also want to support adding
marks manually. These can be thought of like "bookmarks" - a user indicated
region that means something to the user.

* [ ] `addMark`: add a mark to the buffer. If there's a selection, place the
  mark covering at the selection. Otherwise, place the mark on the cursor row.
  - [x] `color`: a color for the scrollbar mark. (in [#12948])
  - [ ] `category`: one of `{"prompt", "error", "warning", "success", "info"}`
* [ ] `scrollToMark`
  - [x] `direction`: `["first", "previous", "next", "last"]`  (in [#12948])
  - [ ] `category`: `flags({categories}...)`, default `"all"`. Only scroll to
    one of the categories specified (e.g. only scroll to the previous error,
    only the previous prompt, or just any mark)
  - [ ] [#13449] - `center` or some other setting that controls how the mark is scrolled in.
    - Maybe `top` (current) /`center` (as proposed) /`nearestEdge` (when
      scrolling down, put the mark at the bottom line of viewport , up -> top
      line)?
  - [ ] [#13455] - `highlight`: `bool`, default true. Display a temporary
    highlight around the mark when scrolling to it. ("Highlight" != "select")
    - If the mark has prompt/command/output sections, only select the prompt and command.
    - If the mark has zero width (i.e. the user just wanted to bookmark a line),
      then highlight the entire row.
* [x] `clearMark`: Remove any marks in the selected region (or at the cursor
  position)  (in [#12948])
* [x] `clearAllMarks`: Remove all the marks from the buffer.  (in [#12948])


#### Selecting commands & output

_Inspired by a long weekend of manually copying .csv output from the Terminal to
a spreadsheet, only to discover that we rejected [#4588] some years ago._

* [x] `selectCommand(direction=[prev, next])`: Starting at the active selection
  anchor, (or the cursor if there's no selection), select the command that
  starts before/after this point (exclusive).  Probably shouldn't wrap around
  the buffer.
  * Since this will create a selection including the start of the command,
    performing this again will select the _next_ command (in whatever
    direction).
* [x] `selectOutput(direction=[prev, next])`: same as above, but with command outputs.

A convenient workflow might be a `multipleActions([selectOutput(prev),
copy()])`, to quickly select the previous commands output.

### Per-profile settings

* [x] `autoMarkPrompts`: `bool`, default `false`.  (in [#12948])
* [ ] `showFindMatchesOnScrollbar`: `bool`, default `true`.
* [ ] `showMarksOnScrollbar`: `bool` or `flags({categories}...)`
  * As an example: `"showMarksOnScrollbar": ["error", "success"]`).
  * Controls if marks should be displayed on the scrollbar.
  * If `true`/`"all"`, then all marks are displayed.
  * If `false`/`"none"`, then no marks are displayed.
  * If a set of categories are provided, only display marks from those categories.
  * [x] the bool version is (in [#12948])
  * [ ] The `flags({categories}...)` version is not done yet.
* [ ] `showGutterIcons`, for displaying gutter icons.

## UX Design

An example of what colored marks look like:

![Select the entire output of a command](https://user-images.githubusercontent.com/18356694/207696859-a227abe2-ccd4-4b81-8a2c-8a22219cd0dd.gif)

This gif demos both prompt marks and marks for search results:

![](https://user-images.githubusercontent.com/18356694/191330278-3f6bc207-1bd5-4ebd-bb0e-1f84b0170f49.gif)


### Gutter icons

![](vscode-shell-integration-gutter-mark.png)
_An example of what the icons in the VsCode gutter look like_

### Multiple marks on the same line

When it comes to displaying marks on the scrollbar, or in the margins, the
relative priority of these marks matters. Marks are given the following
priority, with errors being the highest priority.
* Error
* Warning
* Success
* Prompt
* Info (default)

## Work needed to get marks to v1

* [x] Clearing the screen leaves marks behind
  * [x] Make sure `ED2` works to clear/move marks
  * [x] Same with `ED3`
  * [x] Same with `cls` / `Clear-Host`
  * [x] Clear Buffer action too.
* [X] Circling doesn't update scrollbar
  * I think this was fixed in [#14341], or in [#14045]
* [ ] Resizing / reflowing marks
* [x] marks should be stored in the `TextBuffer`

## Future Considerations
* adding a timestamp for when a line was marked?
* adding a comment to the mark. How do we display that comment? a TeachingTip on
  the scrollbar maybe (actually that's a cool idea)
* adding a shape to the mark? Terminal.app marks prompt lines with brackets in
  the margins
* Marks are currently just displayed as "last one in wins", they should have a
  real sort order
* Should the height of a mark on the scrollbar be dependent on font size &
  buffer height? I think I've got it set to like, a static 2dip, but maybe it
  should represent the actual height of the row (down to a min 1dip)
* [#13455] - highlight a mark when scrolled to with the `scrollToMark` action.
  This is left as a future consideration to figure out what kind of UI we want
  here. Do we want to highlight
  - the prompt?
  - the whole row of the prompt?
  - the prompt and the command?
  - The whole prompt & command & output?
* an `addBookmark` action: This one's basically just `addMark`, but opens a prompt
  (like the window renamer) to add some text as a comment. Automatically
  populated with the selected text (if there was some).
  - A dedicated transient pane for displaying non-terminal content might be
    useful for such a thing.
  - This might just make more sense as a parameter to `addMark`.
* Other ideas for `addMark` parameters:
  - `icon`: This would require us to better figure out how we display gutter
    icons. This would probably be like, a _shape_ rather than an arbitrary
    image.
  - `note`: a note to stick on the mark, as a comment. Might be more valuable
    with something like `addBookmark`.

### Gutter icons

VsCode implements a set of gutter icons to the left of the buffer lines, to
provide a UI element for exposing some quick actions to perform, powered by
shell integration.

Gutter icons don't need to implement app-level actions at all. They _should_ be
part of the control. At least, part of the UWP `TermControl`. These are some
basic "actions" we could add to that menu. Since these are all attached to a
mark anyways, we already know what mark the user interacted with, and where the
start/end already is.
* Copy command
* Copy output
* Re-run command
* Save as task
* Explain this (for errors)

To allow comments in marks (ala "bookmarks"), we can use
the gutter flyout to display the comment, and have the tooltip display that
comment.

This is being left as a future consideration for now. We need to really sit and
consider what the UX is like for this.
* Do we just stick the gutter icons in the margin/padding?
* Have it be a separate space in the "buffer"
  - If it's in the buffer itself, we can render it with the renderer, which by
    all accounts, we probably should.

### Edge cases where these might not work as expected

Much of the benefits of shell integration come from literal buffer text parsing.
This can lead to some rough edge cases, such as:

* the user presses <kbd>Ctrl+V</kbd><kbd>Escape</kbd> to input an ESC character
  and the shell displays it as `^[`
* the user presses <kbd>Ctrl+V</kbd><kbd>Ctrl+J</kbd> to input an LF character
  and the shell displays it as a line break
* the user presses <kbd>Enter</kbd> within a quoted string and the shell
  displays a continuation prompt
* the user types a command including an exclamation point, and the shell invokes
  history expansion and echoes the result of expansion before it runs the
  command
* The user has a prompt with a right-aligned status, ala
  ![](https://user-images.githubusercontent.com/189190/254475719-5007df07-6cc3-42e8-baf7-2572579eb2b9.png)

In these cases, the effects of shell integration will likely not work as
intended. There are various possible solutions that are being explored. We might
want to in the future also use [VsCode's extension to the FTCS sequences] to
enable the shell to tell the terminal the literal resulting commandline.

There's been [other proposals] to extend shell integration features as well.

### Rejected ideas

There was originally some discussion as to whether this is a design that should
be unified with generic pattern matchers. Something like the URL detection,
which identifies part of the buffer and then "marks" it. Prototypes for both of
these features are going in very different directions, however. Likely best to
leave them separate.

## Resources

### Other related issues

Not necessarily marks related, but could happily leverage this functionality.

* [#5916] and [#12366], which are likely to be combined into a single thread
  - Imagine a trigger that automatically detects `error:.*` and then marks the line
* [#9583]
  - Imagine selecting some text, colorizing & marking it all at once
  - `addMark(selection:false)`, above, was inspired by this.
* [#7561] (and broadly, [#3920])
  - Search results should maybe show up here on the scrollbar too.
* [#13455]
* [#13449]
* [#4588]
* [#14754] - A "sticky header" for the `TermControl` that could display the
  previous command at the top of the buffer.
* [#2226] - a scrollable "minimap" in te scrollbar, as opposed to marks

### Relevant external docs
* **GREAT** summary of the state of the ecosystem: https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/28
* https://iterm2.com/documentation-escape-codes.html
  - `OSC 1337 ; SetMark ST` under "Set Mark"
  - under "Shell Integration/FinalTerm
* https://support.apple.com/en-ca/guide/terminal/trml135fbc26/mac
  - discusses auto-marked lines on `enter`/`^C`/`^D`
  - allows bookmarking lines with selection
  - bookmarks can have a name (maybe not super important)
* [How to Use Marks in OS X’s Terminal for Easier Navigation](https://www.howtogeek.com/256548/how-to-use-marks-in-os-xs-terminal-for-easier-navigation/)
* [Terminal: The ‘\[‘ Marks the Spot](https://scriptingosx.com/2017/03/terminal-the-marks-the-spot/)
* Thread with VsCode (xterm.js) implementation notes: https://github.com/microsoft/terminal/issues/1527#issuecomment-1076455642
* [xterm.js prompt markup sequences](https://github.com/microsoft/vscode/blob/39cc1e1c42b2e53e83b1846c2857ca194848cc1d/src/vs/workbench/contrib/terminal/browser/xterm/shellIntegrationAddon.ts#L50-L52)
* [VsCode command tracking release notes](https://code.visualstudio.com/updates/v1_22#_command-tracking), also [Terminal shell integration](https://code.visualstudio.com/updates/v1_65#_terminal-shell-integration)
* ConEMU:
  Sequence | Description
  -- | --
  ESC ] 9 ; 12 ST | Let ConEmu treat current cursor position as prompt start. Useful with PS1.

* https://iterm2.com/documentation-one-page.html#documentation-triggers.html"

### Footnotes

<a name="footnote-1"><a>[1]: Intuitively, this feels prohibitively expensive,
but you'd be mistaken.

An average device right now (I mean something that was alright about 5 years
ago, like an 8700k with regular DDR4) does about 4GB/s of random, un-cached
memory access. While low-end devices are probably a bit slower, I think 4GB/s is
a good estimate regardless. That's because non-random memory access is way way
faster than that at around 20GB/s (DDR4 2400 - what most laptops had for the
last decade).

Assuming a 120 column * 32k line buffer (our current maximum), the buffer would
be about 21MB large. Going through the entire buffer linearly at 20GB/s would
take just 1ms (including all text and metadata). If we assume that each row has
a mark, that marks are 36 byte large and assuming the worst case of random
access, we can go through all 32k within about 0.3ms.



_(Thanks lhecker for these notes)_


[#1527]: https://github.com/microsoft/terminal/issues/1527
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#1527]: https://github.com/microsoft/terminal/issues/1527
[#6232]: https://github.com/microsoft/terminal/issues/6232
[#2226]: https://github.com/microsoft/terminal/issues/2226
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#13163]: https://github.com/microsoft/terminal/issues/13163
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#13455]: https://github.com/microsoft/terminal/issues/13455
[#13449]: https://github.com/microsoft/terminal/issues/13449
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#4588]: https://github.com/microsoft/terminal/issues/4588
[#5804]: https://github.com/microsoft/terminal/issues/5804
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#12948]: https://github.com/microsoft/terminal/issues/12948
[#13455]: https://github.com/microsoft/terminal/issues/13455
[#5916]: https://github.com/microsoft/terminal/issues/5916
[#12366]: https://github.com/microsoft/terminal/issues/12366
[#9583]: https://github.com/microsoft/terminal/issues/9583
[#7561]: https://github.com/microsoft/terminal/issues/7561
[#3920]: https://github.com/microsoft/terminal/issues/3920
[#13455]: https://github.com/microsoft/terminal/issues/13455
[#13449]: https://github.com/microsoft/terminal/issues/13449
[#4588]: https://github.com/microsoft/terminal/issues/4588
[#14341]: https://github.com/microsoft/terminal/issues/14341
[#14045]: https://github.com/microsoft/terminal/issues/14045
[#14754]: https://github.com/microsoft/terminal/issues/14754
[#14341]: https://github.com/microsoft/terminal/issues/14341

[VsCode's extension to the FTCS sequences]: https://code.visualstudio.com/docs/terminal/shell-integration#_vs-code-custom-sequences-osc-633-st

[other proposals]: https://gitlab.freedesktop.org/terminal-wg/specifications/-/merge_requests/6#f6de1e5703f5806d0821d92b0274e895c4b6d850
