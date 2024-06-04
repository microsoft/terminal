
# Mouse bindings

## Abstract

We've had numerous requests to configure how the mouse behaves in the Terminal.
The original behavior was a simple duplication of how conhost behaved: a right
click will copy the a selection if there is one, or paste the clipboard if there
isn't. Over time, we've accumulated a number of scenarios that we believe can
all be addressed by allowing more fine-grained mouse binding support. However,
upon further review, ultimately, we don't really need _deep_ mouse binding
support.

### Scenarios

The following is a list of all the feature requests we've linked to mouse
bindings in the past, grouped into categories of related requests:

### Change how mouse multi-click selects
* [ ] [#7173] Multiple sets of word delimiters for double-click selection
* [ ] [#9881] Limit triple-click selection to command field only
* [ ] [#6511] Multi-click selection granularity
* [ ] [#3196] Feature Request: Smart Double-click Selection (regeces?)

### Change the action that L/M/R-mouse performs
* [ ] [#7646] xterm-style selection, paste on middle click, copy on mouse release
* [ ] [#10802] - `VK_XBUTTON1/2`, etc.
* [ ] [#6250] - separate "Paste Clipboard" vs "Paste Selection" actions
* [x] [#3337] - Add a right-click context menu

### Other
These are smaller, independent features that could all have an individual setting (if needed)

* [ ] [#11710] Request: Setting to disable zooming with ctrl+mousewheel
* [ ] [#13598] Add option to configure URL clicking behavior
* [ ] [#11906] Option to disable "Pinch to Zoom"
* [ ] [#6182] Fast scroll by holding down Alt while scrolling with mouse wheel
* [ ] Block selection by default (without `alt`) (see mail thread "RE: How to disable line wrap selection in Terminal")


## Solution design

Following the above scenarios, solutions are proposed below:

### Change how mouse multi-click selects

Across the requests here, we've got the following requests:

> * double-click: selects a "word" between 2 same delimiters
> * triple-click: selects an entire string separated by spaces
> * 4-click: entire line

> Currently, Ctrl+A will select the entire command/text input, however, triple
> clicking (mouse version of Select All selects the entire line (including the
> prompt). GIF shows selecting with mouse vs with keyboard:
> ...
> I would like the triple click action to align to the Ctrl+A selection method.

> Could we maybe add shift double click to select using alternate word
> delimiters?

> I was really thinking more of regex though, because it can be a good starting
> point for implementing more advanced features like type-specific smart
> highlighting and hyperlinking of terminal text, not just smart selection.

To boil this down, users want to be able to configure the behavior of double,
triple, and quadruple clicks. The most common request is to change the
delimiters for double-click selection. But users also want to be able to
configure the delimiters to _change_ on
<kbd>Shift</kbd>/<kbd>Alt</kbd>/<kbd>Ctrl</kbd> clicks.


```json
"mouse": {
    "clicks": {
        { "click": "double", "command": { "action": "expandSelection", "delimeters": " /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502" } }
        { "click": "shift+double", "command": { "action": "expandSelection", "delimeters": " " } }
        { "click": "triple", "command": { "action": "expandSelection", "regex": "^.*$" } }
    }
}
```

Alternatively,

```json
"mouse": {
    "doubleClick": " /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502",
    "tripleClick": { "regex": "^.*$" }
}
```

[#3337]: https://github.com/microsoft/terminal/issues/3337
[#6182]: https://github.com/microsoft/terminal/issues/6182
[#6250]: https://github.com/microsoft/terminal/issues/6250
[#6511]: https://github.com/microsoft/terminal/issues/6511
[#7173]: https://github.com/microsoft/terminal/issues/7173
[#7646]: https://github.com/microsoft/terminal/issues/7646
[#9881]: https://github.com/microsoft/terminal/issues/9881
[#10802]: https://github.com/microsoft/terminal/issues/10802
[#11710]: https://github.com/microsoft/terminal/issues/11710
[#11906]: https://github.com/microsoft/terminal/issues/11906
[#13598]: https://github.com/microsoft/terminal/issues/13598
