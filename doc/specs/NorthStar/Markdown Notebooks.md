---
author: Mike Griese @zadjii-msft
created on: 2022-05-18
last updated: 2022-10-28
issue id: n/a
---

# Markdown Notebooks in the Terminal

## Abstract

"Terminal Notebooks" would allow developers to combine documentation and code in
a single file. Blocks of commands could be grouped logically together with their
output, for a more semantic experience. By leveraging markdown as a format, we
can expand upon an broad ecosystem of pre-existing content developers are
already familiar with.

## Background

Notebooks have risen to popularity in recent years as a way of combining both
code and documentation in a single experience. They enable users to seamlessly
write code, and execute it to see the output, write documentation on the code,
and share that with others. However, there's not really anything like notebooks
for generic commandline experiences.

There are, however, markdown files. Markdown has become a bit of a lingua franca
of the developer experience. It's used prominently on GitHub - the "homepage" of
any repo on GitHub is now typically a markdown file. This file will have all
sorts of documentation, and notably these READMEs are often filled with commands
that one can execute for this project. Downloading, installing its dependencies,
building and running the project, etc, all commands that are already listed
today in READMEs across the world.

It would be a major convenience for users to be able to just load a pre-rendered
markdown file directly into their terminal windows. These files already include
marked blocks of code which identify sets of commands for the command line. It
should be as simple as clicking a button to run these commands in the Terminal,
or even to run a whole file worth of commands automatically.

### Inspiration

[Jupyter notebooks] ([a Jupyter example]) served as the primary inspiration for
this feature. Another shoutout to [this comment on HackerNews], which inspired a
lot of brainstorming on this topic over the last year since it was posted.

Many initial brainstorms were focused on more notebook-like features in the
Terminal. For example, finding ways to create individual terminal blocks inline
with commands, where each input command and its output would be separated from
one another, possibly separated by some sort of text/rich markup. This seemed to
precipitate the need for a new file syntax to be authored, where we could save
commands as they were run. The Terminal would then open this new file type as a
set of terminal blocks each pre-populated with these saved commands.

However, this came with the drawback that projects which would like to leverage
this feature would have to author entirely new files, in a new syntax, just to
make use of this functionality. It seemed as though it was a niche enough UX
that it would be unlikely to broadly catch on.

The real inspiration here was that there's already a file type with broad
adoption that's already filled with commands like this. Markdown files. Take a
look at something like
[building.md](https://github.com/microsoft/terminal/blob/main/doc/building.md).
That file _already_ has a long set of commands for building the Terminal,
running tests, deploying, and various other helper scripts. Being able to
immediately leverage this existing ecosystem would undobtably lead to quicker
adoption.

### User Stories

* **A**: The user can perform some commandline action (like `wt open
  README.md`), which opens a new pane in the Terminal, with the markdown file
  rendered into that pane.
* **B**: Markdown panes have buttons next to code blocks that allow the text of
  that block to be sent directly to the adjacent terminal as input.
* **C**: The user can press different buttons to run some subset of all the
  commands in the file
  - **C.1**: Run all the commands in this file
  - **C.2**: Run all the commands from (the currently selected block) to the end
    of the file
  - **C.1**: Run all the commands from (the currently selected block) to the
    next header. (e.g., run all the commands in this section of the doc.)
* **D**: The user can edit the contents of the markdown file directly in the
  Terminal.
* **E**: The Terminal could be configured to automatically open a markdown file
  when `cd`ing to a directory
* **F**: The command for opening a markdown file also supports opening files
  from the web (e.g. directly from GitHub)
* **G**: Code blocks in the markdown file are automatically assigned
  autoincrementing IDs. The user can perform an action (via keybinding, command
  palette, whatever) to execute a command block from the file, based on it's
  assigned ID.
* **H**: ...


## Solution Design

Below are two different structures that could be used for imlpementing notebooks
in the Terminal. These are **Side-by-side** notebooks, and **Inline** notebooks.

* **Side-by-side** notebooks consist of two panes - a standard terminal control
in one, and rendered markdown in the other.
* **Inline** notebooks are more like traditional notebooks, with the terminal
output rendered as a block within the rendered markdown content.

### Side-by-side notebooks

Opening Markdown side-by-side with the Terminal output is certainly a little
different than the way a notebook traditionally works. Notebooks typically have
the code in a block, with output inline, below the block. Blocks could also just
be dedicated to text, for documentation mixed between the code. The feature
proposed here is different from that, for sure. For this proposal, the Terminal
still exists side-by-side from the source markdown. Running commands from the
markdown text would then send the command as a string of input to the connected
terminal. This approach was elected over attempting to create artificial
boundaries between different blocks.

Oftentimes, the command line is a very stateful experience. Set some environment
variables, run some script, use the errorlevel from the previous command, etc.
Running each block in wholly separate console instances would likely not be
useful.

Additionally, finding the separation between command line input and its output,
and the separation between individual commands is not an entirely trivial
process. Should we try to separate out the command input line into one buffer,
then the output into another buffer sounds great on paper. Consider, however,
something like `cmd.exe`, which does not provide any sort of distinction between
its input line and its output. Or `python.exe`, as an interactive REPL, which
certainly doesn't tell the terminal the difference. How would we be able to
detect something like a multi-line command at the REPL?

By keeing the command blocks out-of-band from the terminal output, we keep the
familiar terminal experience. It acts just as you'd expect, with no additional
configuration on the user's side. The commands are something that are already
written down, just waiting for the user to run them. They could even be sent to
something that isn't necessarily a shell - like pasting a bit of configuration
into a text editor like `vim` or `emacs`. The commands in the markdown side are
just strings of text to send to the terminal side - nothing more.

### Inline notebooks

Is there a way to have a more traditional notebook experience, where the
terminal output is rendered as blocks within the markdown itself? I believe
there is, by making agressive use of the FTCS mark VT sequences. These are
sequences that enable identifying the parts of the buffer as a prompt, the
commandline, or the output. We can use these to pull out individual rows of the
buffer, and display them as miniature terminal controls within the notebook.

This involves a single terminal insance backing the entire notebook. When the
user runs a code block in the notebook, we'll `sendInput` the text to the
backing terminal the same as before. We'll then use the `FTCS_COMMAND_EXECUTED`
sequence to know where the actual start of the output of the command is. We'll
gather all output from that moment on, and tee that to a separate "front"
control, which we use as the display for the output of the command. We'll render
that "front" control inline with the rest of the markdown content, immediately
below the code block for that command.

> **Warning**: TODO! How do we want to handle interactive notebooks? Like, the user
> wants to run a command, and that creates a new block? We want to make sure
> the shell prompt is still just as interactive as ever.

> **Warning**: TODO! What about users who want to hit play on a whole bunch of
> blocks at once? We need a mechanism for queueing them, so that pressing play
> on two blocks waits till the first completes before starting the second.

> **Warning**: TODO! How do we deal with "page editing" multiple commands,
> interactively? Like, The user wants to interactively compose `cd
> c:\foo\`<kbd>tab</kbd>, then as a part of the same block do `dir | findstr
> *.json`? Does each command become it's own block, with a button to
> "concatenate" to the preceeding / following block?


### Windows Terminal implementation

The Terminal will allow for non-terminal content in a pane. This is something
that's been prototyped before, just needs a stronger justification for
finishing.

We'll leverage the Terminal's existing `sendInput` command to handle a lot of
this. That can be used to send keystrokes to the Terminal. Figuring out which
pane to send the `sendInput` command to might be a bit tricky. We'll need to
figure out what an action like that does when the active pane is not a terminal
pane.

The side-by-side experience is the easiest to implement today with the current
Terminal model. This wouldn't require any major changes to our `TermControl`,
which implements the actual terminal control within the Terminal app.

The inline experience would be more complicated - how do we

## UI/UX Design

### Side-by-side
![A rough mockup of what this feature might look like](img/mockup-000.png)


### Inline

![](img/inline-blocks-000.png)

## Tenents

<table>

<tr><td><strong>Accessibility</strong></td><td>

[comment]: # How will the proposed change impact accessibility for users of screen readers, assistive input devices, etc.

</td></tr>

<tr><td><strong>Security</strong></td><td>

[comment]: # How will the proposed change impact security?

Opening a file like this will _never_ auto-run commands. Commands must always be
intentionally interacted with, to provide a positive confirmation from the user
"yes, I intended to run `curl some.website.com/foo.txt`".

</td></tr>

<tr><td><strong>Reliability</strong></td><td>

[comment]: # Will the proposed change improve reliability? If not, why make the change?

</td></tr>

<tr><td><strong>Compatibility</strong></td><td>

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

It's critically important that nothing about this feature be necessarily Windows
Terminal-dependent. These features shouldn't be powered by some new undocumented
escape sequence that only we support. They should NOT be powered by new Windows
APIs, especially not any extensions to the Console API. There's no reason other
terminals couldn't also implement similar functionality.

</td></tr>

<tr><td><strong>Performance, Power, and Efficiency</strong></td><td>

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

</td></tr>

</table>

## Potential Issues

For rendering markdown, we'll either need:
* A way to display a WebView in a WinUI2  XAML Island
  - This is something that's on the backlog currently for MUX 2.x. Theoretically
    not too hard to add an `IInitializeWithWindow` to `WebView2` which should
    enable XAML Islands, but needs more research.
* To migrate to WinUI 3
  - In WinUI 3 I believe we should be able to get WebViews for free.
  - We might still be a XAML Island in WinUI 3, which may complicate that.
* A C++ based method of rendering Markdown to UWP XAML
  - There's a Windows Community Toolkit control for rendering to XAML currently,
    but that is backed by C#, so we can't use that.

We'll also need the markdown rendering to be extensible, so that we can insert
"play" buttons alongside the blocks.

## Future considerations

### Tighter GitHub integration

GitHub already has the helpful "Open In GitHub" button for opening a repo in the
GitHub desktop client, or in Visual Studio.

![GitHub's "Clone, open or download" flyout](img/GitHub-open-with.png)

It'd be cool if there was a similar button for opening it up in the Terminal. It
could open the README immediately as a new tab, and then provide some sort of
InfoBar with a button that would allow the user to immediately clone the repo to
some location on their PC. This would likely need a protocol handler installed
by the Terminal to help connect the browser to the Terminal.

### Collapsible Blocks

One of the key features of notebooks is the ability to easily collapse regions
of the notebook. With the command output being out of band from the input of the
command, not as independent blocks, this becomes a bit trickier. To try and
reproduce a similar ability to collapse regions of the buffer, we'll look to
[Marks] in the terminal as a potential solution. The FinalTerm sequences allow a
client to mark up the region of the buffer that's the prompt, the command line,
and the output. Using those marks would provide an easy heuristic to allow users
to collapse the output of commands. These sequences however do require manual
configuration by the user, and are not expected to be able to work in all
environments (and shells). While powerful, because of this limitation, we didn't
want to architect the entire experience around something that wouldn't always
work.

## Resources

[comment]: # Be sure to add links to references, resources, footnotes, etc.


### Examples

[This thread on the Terminal repo](https://github.com/microsoft/terminal/issues/14680), where the user included a long collection of commands to run

### Footnotes

<a name="footnote-1"><a>[1]:

[Jupyter notebooks]: https://jupyter.org/
[a Jupyter example]: https://jupyter.org/try-jupyter/retro/notebooks/?path=notebooks/Intro.ipynb
[this comment on HackerNews]: https://news.ycombinator.com/item?id=26617656
[Marks]: https://github.com/microsoft/terminal/issues/11000
