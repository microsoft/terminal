---
author: Mike Griese
created on: 2023-01-26
last updated: 2023-01-26
issue id: n/a
---
# Windows Terminal Copilot | Overview

## Abstract

GitHub Copilot is a fairly revolutionary tool that offers complex predictions
for code from the context of the file you're working on and some simple
comments. However, there's more potential to use it outside of just the text
editor. Imagine integration directly with the commandline, where Copilot can
offer suggestions based off of descriptions of what you'd like to do. Recent
advances in AI models can enable dramatic new features like this, which can be
added to the Terminal.

## Background

Imagine Copilot turning "get the process using the most CPU" into `Get-Process |
Sort-Object CPU -Desc | Select-Object ID, Name, CPU -First 1`. Both [Fig] and
[Warp] have produced similar compelling user experiences already, powered by AI.
Github Labs are also working on a similar natural language-to-command model with
[Copilot CLI].

Or imagine suggestions based off your command history itself - I just ran `git
add --all`, and Copilot can suggest `git commit ; git push ; gh pr create`. It
remains an open question if existing AI models are capable of predicting
commands based on what the user has previously done at the command line. If it
isn't yet possible, then undoubtably it will be possible soon. This is an
idealized future vision for AI in the Terminal. Imagine "**Intelli**sense for
the commandline, powered by artificial **intelligence**"

Another scenario that current models excel at is explaining code in natural
human language. The commandline is an experience that's frequently filled with
esoteric commands and error messages that might be unintuitive. Imagine if the
Terminal could automatically provide an explanation for error messages right in
the context of the Terminal itself. No need to copy the message, leave what
you're doing and search the web to find an explanation - the answer is right
there.

### Execution Strategy

Executing on this vision will require a careful hand. As much delight as this
feature might bring, it has equal potential for PR backlash. Developers already
hate the concept of "telemetry" on Windows. The idea that the Windows Terminal
has built-in support for logging _every command_ run on the command line, and
sending it to a Microsoft server is absolutely a recipe for a PR nightmare.
Under no circumstances should this be built directly in to the Terminal.

This doc outlines how the Terminal might enable this functionality via a "GitHub
Copilot Extension". Instead of building Copilot straight into the Terminal, it
would become an optional extension users could install. By making this
explicitly a "GitHub Copilot" branded extension, it's clear to the users how the
extension is maintained and operated - it's not a feature of _Windows_, but
instead a _GitHub_ feature.

### User Stories

When it regards Copilot integration in the Terminal, we're considering on the following four scenarios.

1. **[Prompting]**: The User types a prompt, and the AI suggests some commands given that prompt
   - For example, the user literally types "give me a list of all processes with
     port 12345 open", and that prompt is sent to the AI model to generate
     suggestions.
2. **[Implicit Suggestions]**: A more seamless suggestion based solely on what the user has already typed
   - In this scenario, the user can press a keybinding to summon the AI to
     suggest a command based solely on the contents of the buffer.
   - This version will more heavily rely on [Shell Integration]
   - This will be referred to as **"Implicit suggestions"**
3. **"[Explain that]"**: Highlight some command, and ask Copilot to explain what it does.
   - Additionally, a quick icon that appears when a command fails, to ask AI to
     try and explain what an error means.
4. Long lived context - the AI learns over time from your own patterns, and
   makes personalized suggestions.

For the sake of this document, we're going to focus on the first three
experiences. The last, while an interesting future idea, is not something we
have the engineering resources to build. We can leverage existing AI models for
the first three in all likelihood.

Each of the first three scenarios is broken down in greater detail in their linked docs.


The following plan refers to specifically overarching elements of the Copilot
extension, which are the same regardless of individual features of the
extension. This list was made with consideration for what's possible _before
Build 2023_, alongside what we want to do _in the fullness of time_.

#### By Build

Story |  Size | Description
--|-----------|--
A | 🐣 Crawl  | The Terminal can use a authentication token hardcoded in their settings for OpenAI requests
A | 🚶 Walk  | The Terminal can load the user's GitHub identity from Windows

#### After Build

Story |  Size | Description
--|-----------|--
A | 🐣 Crawl  | The Terminal can load in-proc extensions via Dynamic Dependencies
A | 🚶 Walk   | Terminal Extensions can provide their own action handlers
A | 🚶 Walk   | Terminal Extensions can query the contents of the text buffer
A | 🚶 Walk   | [Shell integration] marks can be used to help make AI suggestions more context-relevant
A | 🏃‍♂️ Run    | Extensions can provide their own UI elements into the Terminal
A | 🏃‍♂️ Run    | Copilot is delivered as an extension to the Terminal
A | 🚀 Sprint | The Terminal supports a status bar that shows the state of the Copilot extension

> **Warning**: TODO! How much of this spec should be the "extensions" spec, vs the
> "copilot" spec? Most of the "work" described by this spec is just "Make
> extensions work". Might want to flesh out that one then.

#### North star user experience

As the user is typing at the commandline, suggestions appear as they type, with
AI-driven suggestions for what to complete. These suggestions are driven by the
context of the commands they've previously run (and possibly other contents of
the buffer).

The user can highlight parts of a command that they don't understand, and have
the command explained in natural language. Commands that result in errors can
provide a menu for explaining what the error is, and how to remedy the issue.

### Elevator Pitch

It's Copilot. For the command line.

## Business Justification

It will delight developers.

## Scenario Details

"AI in the Terminal" covers a number of features each powered by AI. Each of
those features is broken into their own specs (linked above). Please refer to
those docs for details about each individual scenario.

This doc will largely focus on the overarching goal of "how do we deliver
Copilot in the Terminal?".

### Implementation Details

#### Github Authentication
<sup>_By Build 2023_</sup>

We don't know if this will be powered by Github Copilot, or some other
authentication method. This section is left blank while we await those answers.

> **Warning**: TODO! do this

#### Extensions implementation
<sup>_After Build 2023_</sup>

> **Warning**: TODO! do this

Extensions for the Terminal are possible made possible by [Dynamic Dependencies for Main packages]. This is a new feature in Windows SV2 (build 22533 I believe). This enables the Terminal to "pin" another application to the Terminal's own package graph, and load binaries from that package.

Main Packages can declare themselves with the following:
```xml
<Package>
  <Properties>
    <uap15:dependencyTarget>true</uap15:dependencyTarget>
  </Properties>
</Package>
```

This is a new property in the SV2 SDK. That'll allow them be a target of a
Dynamic Dependency. This means that **extensions will be limited to users
running SV2+ builds of Windows**.

```xml
<Package>
    <Properties>
        <uap15:dependencyTarget>true</uap15:dependencyTarget>
    </Properties>
    <Applications>
        <Application Id="App"
                     Executable="$targetnametoken$.exe"
                     EntryPoint="$targetentrypoint$">
            <Extensions>
                <uap3:Extension Category="windows.appExtension">
                    <uap3:AppExtension Name="com.microsoft.windows.terminal.extension"
                                        Id="MyTerminalExtension"
                                        DisplayName="...">
                        <uap3:Properties>
                            <!-- TODO! Determine what properties we want to put in here -->
                            <Clsid>{2EACA947-FFFF-4CFA-BA87-BE3FB3EF83EF}</Clsid>
                        </uap3:Properties>
                    </uap3:AppExtension>
                </uap3:Extension>
            </Extensions>
        </Application>
    </Applications>
</Package>
```

#### Consuming extensions from the Terminal
<sup>_After Build 2023_</sup>

> **Warning**: TODO! do this

## Tenents & Potential Issues

See the individual docs for compatibility, accessibility, and localization
concerns relevant to each feature.

## To-do list

> **Note**: Refer to the individual docs for more detailed plans specific to
> each feature. This section is dedicated to covering only the broad tasks that
> are relevant to the Copilot extension as a whole.

## Before Build Todo's
### 🐣 Crawl
* [ ]  Allow the user to store their OpenAI API key in the `settings.json`,
  which we'll use for authentication
  * This is just a placeholder task for the sake of prototyping, until a real
    authentication method is settled on.

### 🚶 Walk
* [ ] Actually do proper authentication.
  * This might be through a Github device flow, or a DevID login.
  * Remove the support for just pasting the API key in `settings.json` at this point.

## After Build Todo's

> **Warning**: TODO! Almost everything here is just "enable extensions". That might deserve a separate spec.

### 🐣 Crawl
* [ ]

### 🚶 Walk
* [ ]

### 🏃‍♂️ Run
* [ ]

### 🚀 Sprint
* [ ]

## Conclusion

### Future Considerations

#### Shell-driven AI

This document focuses mainly on Terminal-side AI features. We are not precluding
the possiblity that an individual shell may want to implement AI-driven
suggestions as well. Consider PowerShell - they may want to deliver AI powered
suggestions as a completion handler. We will want to provide ways of helping to
promote their experience, rather than focus on a single implementation.

The best way for us to help elevate their experience would be through the
[Suggestions UI] and [shell-driven autocompletion]. This will allow us to
promote their results to a first-class UI control. This is a place where we can
work together better, rather than trying to pick one singular design in this
space and discarding the others.

Similarly, [Copilot CLI] could deliver their results as [shell-driven
autocompletion], to further elevate the experience in the terminal.

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

[Implicit Suggestions]: ./Implicit-Suggestions.md
[Prompting]: ./Prompting.md
[Explain that]: ./Explain-that.md
<!-- TODO! -->
[shell-driven autocompletion]: ../Terminal-North-Star.md#Shell_autocompletion
[Dynamic Dependencies for Main packages]: TODO!
