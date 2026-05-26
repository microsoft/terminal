---
author: Mike Griese @zadjii-msft
created on: 2026-05-26
last updated: 2026-05-26
issue id: n/a
---

# Windows Terminal - Snippet Parameters  <!-- aka "input variables", "snippet args" -->

## Abstract

This spec proposes adding **parameters** (a.k.a. "input variables", a.k.a.
"snippet args") to `sendInput` snippets. A snippet author can today encode a
long, frequently-used commandline — `ssh user@host`, `docker run …`, `git
checkout some-branch` — and recall it instantly from the Suggestions UI. What
they _can't_ do is leave a hole in that commandline for the user to fill in
when they invoke the snippet. With parameters, they can. The Suggestions UI
becomes a tiny in-place form: you pick the snippet, you type the bits that
change, you hit <kbd>Enter</kbd>, the resolved command is sent to the shell.

## Background

> [!NOTE]
> This is an addenda to the original [Snippets] spec. Read that one first — this
> doc assumes you're already familiar with how `sendInput` snippets work today,
> what the `.wt.json` mechanism is, and how the [Suggestions UI] dispatches a
> snippet into the active `TermControl`.

### Inspiration

The original [Snippets] spec called this out as a 🚀 Sprint item:

> **I** | 🚀 Sprint | Snippets can have prompt-able sections of input

…and tracked it as the elusive [#12927]. Two years of looking at real users'
`.wt.json` files later, it's clear this is the single largest gap in the
snippets story. Almost every snippet I see in the wild either:

1. hard-codes some value that the user has to remember to edit after pasting
   (`ssh zadji@my-dev-box-2024`, then back-arrow to fix the year), or
2. is some flavor of "almost-a-snippet" — a shell alias or function the user
   wrote precisely because snippets couldn't take an argument.

Both of these are us telling the user "go figure it out yourself". We can do
better.

There's a clear north star here. **VsCode's [input variables]** are the model
worth copying. They get three things right:

1. The substitution token lives _inline_ in the command string
   (`${input:variableName}`), right where it'll be used. You don't have to
   look elsewhere to figure out where a parameter goes.
2. The parameter _metadata_ (description, default, type) lives in a sibling
   `inputs` array on the task definition. That keeps the inline token short
   and readable, while still letting authors annotate each parameter as
   richly as they want.
3. There are exactly three input types in v1: free-form string
   (`promptString`), pick-from-a-list (`pickString`), and "run a command
   and use the output" (`command`). That's a deliberate, small surface that
   covers ~90% of real use cases.

**Warp [workflows]** also do this, but at the other end of the design space.
They encode arguments with `{{name}}` mustache-style placeholders in the
command string, and let authors declare `name`, `description`, and
`default_value` per argument. See e.g.
[`c_url_a_url_and_follow_redirects.yaml`][warp-curl] — a tiny example, but it
shows exactly the shape of the user experience: "here's a command, here's the
hole, type the value, hit enter." I want our story to feel that approachable.

We've also got our own house prior art here. The original [Command Palette]
spec introduced `${profile.name}`, `${profile.icon}`, and `${scheme.name}`
for command iteration. Those tokens are alive and well today in
`Microsoft.Terminal.Settings.Model::Command.cpp` — see the `ProfileNameToken`,
`ProfileIconToken`, and `SchemeNameToken` constants. **Whatever syntax we
pick for snippet parameters, it should rhyme with `${profile.name}`, not
fight with it.** More on that below.

### User Stories

Story | Size | Description
--|-----------|--
A | 🐣 Crawl  | A snippet author can encode a single string parameter in a `sendInput` snippet. The user is prompted to fill it when they invoke the snippet from the Suggestions UI.
B | 🐣 Crawl  | The same parameter can appear multiple times in one snippet. The user fills it _once_; every occurrence is substituted.
C | 🐣 Crawl  | A snippet can have multiple distinct parameters. The Suggestions UI walks through them in order with <kbd>Tab</kbd>/<kbd>Enter</kbd>, and <kbd>Shift</kbd>+<kbd>Tab</kbd> walks back.
D | 🐣 Crawl  | The Suggestions UI's preview (the terminal's ghost text) updates in real time as the user types into a parameter slot.
E | 🚶 Walk   | A parameter can declare a `default` value, which prefills the search box for that slot.
F | 🚶 Walk   | A parameter can declare a `description`, which is surfaced as placeholder/help text while that slot is being filled.
G | 🏃‍♂️ Run    | Parameters can be `enum`-typed — the author declares an allowed set of values; the Suggestions UI shows them as a picker for that slot.
H | 🏃‍♂️ Run    | Parameters can be CLI-driven — the author declares a command whose stdout becomes the picker's list at invocation time.
I | 🚀 Sprint | A 3p cmdpal extension can serve the parameter-filling UI itself (via the SDK's [`IParametersPage`][devpal-iparameterspage] interface), letting extensions like a file-picker or a git-branch-picker plug straight into snippet invocation.

### Elevator Pitch

_"Stop hard-coding the host name in your `ssh` snippet. Just leave a hole. The
Terminal will ask for it when you run it."_

You write the snippet once, with `${host}` where the variable bit goes.
The Suggestions UI pauses on that slot, you type the host name, you hit
<kbd>Enter</kbd>, the resolved commandline is sent to your shell. No
remembering aliases, no re-editing, no leaving the Suggestions UI to type the
real command.

## Business Justification

Snippets without parameters are a "look up this exact string" feature.
Snippets with parameters are a _composable_ feature — they're how a 5-snippet
list does the work of a 50-snippet list. It will delight developers.

## Scenario Details

### Parameter syntax in the `input` string

I'm proposing **`${<name>}`** as the substitution token. Examples:

```jsonc
{
    "name": "SSH to a dev box",
    "command":
    {
        "action": "sendInput",
        "input": "ssh ${user}@${host}",
        "parameters":
        [
            { "name": "user", "description": "Your account on the dev box" },
            { "name": "host", "description": "Dev box hostname (or alias from ~/.ssh/config)" }
        ]
    }
}
```

```jsonc
{
    "name": "Docker run with a tag",
    "command":
    {
        "action": "sendInput",
        "input": "docker run --rm -it ${image}:${tag}",
        "parameters":
        [
            { "name": "image" },
            { "name": "tag", "description": "Image tag, e.g. latest, 1.0.3" }
        ]
    }
}
```

```jsonc
{
    "name": "Checkout a branch",
    "command":
    {
        "action": "sendInput",
        "input": "git checkout ${branch}",
        "parameters":
        [
            { "name": "branch" }
        ]
    }
}
```

A parameter `name` must match the regex `[a-zA-Z_][a-zA-Z0-9_]*`. Anything
else inside `${…}` that doesn't match a declared parameter (or a known
well-known token like `${profile.name}`) is treated as a literal string and
rendered untouched — which is what users will expect, since this matches how
the existing `${profile.name}` substitution behaves.

#### Why this syntax (and not something else)?

This was the only real design decision in the doc, so let me walk through the
alternatives I considered:

| Candidate | Verdict |
|---|---|
| **`${<name>}`** (proposed) | It matches our existing `${profile.name}` / `${scheme.name}` shape exactly. No new namespace to learn; the curly braces and `$` are the only markers. The tradeoff: without a discriminator namespace, parameter tokens live in the same syntactic space as `${profile.name}` etc. — the substitution helper has to know that anything not matching an existing `profile.` / `scheme.` token is a parameter lookup. It's a small thing, but it keeps the snippet readable. The cost — the substitution helper has to know that anything not matching a known well-known token is a parameter — is a cost we pay once, in the parser, not every time a user writes a snippet. ✅ |
| `${input:name}` | This is VsCode's spelling. I'd love alignment with VsCode here, but the colon-as-namespace-separator doesn't exist anywhere else in WT's settings model. Using it would force us to support _both_ delimiters across our token parser, or risk confusing readers about which goes where. ❌ |
| `{{name}}` | This is Warp's spelling. It has the virtue of being shorter, but it collides visually with many templating engines (Mustache, Handlebars, Jinja, GitHub Actions expressions). It also doesn't compose with our existing `${...}` parser. ❌ |
| `%name%` | The classic cmd.exe / batch environment variable style. Real risk that someone's snippet literally contains the string `%PATH%` and we'd mangle it. ❌ |
| `$name` / `$1` | Shell-style. Same collision problem as above, much worse — _every_ snippet that includes a real shell variable would be broken. ❌ |

So: **`${<name>}`**. It is what someone who has read the original
[Command Palette] spec would guess.

#### Escaping literal `${...}`

In the rare case where a user genuinely wants the literal string
`${foo}` to be sent to the shell, they can backslash-escape the leading
dollar: `\${foo}`. The Terminal will strip the backslash and send the
rest verbatim, and won't try to substitute. This is the same escape strategy
used by every shell on the planet, so it should feel native.

If the snippet has no `parameters` array, we don't do substitution at all
(see [Compatibility](#tenets) below). So in practice this escape only matters
for snippets that _do_ declare parameters but also want to send a literal
`${foo}` somewhere in the same string. I expect roughly zero such
snippets to exist, but we should still spec the behavior.

### The `parameters` array shape

The `parameters` property is an array of objects, each describing one input
slot. v1 schema:

```jsonc
{
    "name": "branch",            // required, identifier used by ${<name>}
    "description": "Branch name" // optional, displayed as placeholder/help text
}
```

That's it for v1. Two fields. The bar to ship is deliberately low.

#### What about `icon`?

Mike asked: does it make sense to put an `icon` on a parameter? I'm leaning
**no, not in v1**, and here's why:

A snippet has an icon today (the icon you see in the Suggestions UI list).
That icon represents _the snippet itself_ — "this is the docker-run one".
While the user is _filling_ a parameter, the snippet is still selected and
that icon is still on screen. A second icon, attached to the parameter slot,
would compete with it for the user's attention without telling them anything
useful: "you're filling in the `tag` parameter [icon of a wrench?]" — what
does the wrench buy us?

There _is_ a case for icons on parameter _values_ — once we add enum-typed
parameters (where each allowed value is its own pickable entry), each value
deserves an icon for the same reason every Suggestions UI list entry does.
That's a future consideration; see below.

So: **no `icon` field on the parameter itself for v1**. I'll happily eat my
words later if a strong use case shows up.

#### Why not put parameter metadata inline?

VsCode famously _doesn't_ — they keep metadata in a separate `inputs` array
on the task, with the inline token being just `${input:name}`. Warp does the
same shape, with `arguments` declared alongside the command.

We're doing the same thing here, for the same reason: a snippet's `input`
string is the part the user reads to figure out _what the snippet does_.
Stuffing per-parameter descriptions or pickers inline would make even simple
snippets unreadable. Compare:

```jsonc
// Hypothetical inline metadata — don't do this
"input": "ssh ${user:description=Your username:default=root}@${host:description=Dev box hostname}"
```

vs.

```jsonc
// What we're proposing
"input": "ssh ${user}@${host}",
"parameters":
[
    { "name": "user", "description": "Your username", "default": "root" },
    { "name": "host", "description": "Dev box hostname" }
]
```

The second one is the one you'd actually want to maintain.

### Parameter-filling state machine in the Suggestions UI

When the user accepts a parameterized snippet from the Suggestions UI, we
don't dispatch it immediately. Instead, we enter what I'll call
**parameter-filling mode**. The state is:

| State | What you see | What keys do |
|---|---|---|
| `Browsing` | The Suggestions UI list of snippets, search box at top, optional preview ghost text in the terminal. _(This is exactly today's behavior.)_ | Type to filter, ↑/↓ to select, <kbd>Enter</kbd> to accept. |
| `Filling[i]` | The Suggestions UI is still open, but the list has collapsed to a single row representing the parameter currently being filled (with its `description` as placeholder text). The search box is empty. The terminal preview shows the snippet with all _previously-filled_ parameters substituted and the current parameter rendered as `▎` (or however we visually distinguish the "cursor is here" slot). | Type into the search box → preview updates live. <kbd>Tab</kbd> or <kbd>Enter</kbd> → commit this slot's value, advance to next. <kbd>Shift</kbd>+<kbd>Tab</kbd> → return to previous slot (preserving its current value). <kbd>Esc</kbd> → cancel the whole thing, close the Suggestions UI. |
| `Filling[last]` → done | Same as above. | <kbd>Enter</kbd> dispatches the resolved input string just like today's `_dispatchCommand` path. |

A few things worth nailing down:

- **The search box is the parameter input.** We're reusing `_searchBox` from
  `SuggestionsControl.xaml`, not introducing a new control. When the state
  transitions from `Browsing` to `Filling[0]`, we clear the search box,
  reset its placeholder to the parameter's `description`, and re-route
  what `_filterTextChanged` does (see [UI/UX Design](#uiux-design) below).
- **The preview is the existing `PreviewAction` event.** Today
  `SuggestionsControl::_selectedCommandChanged` raises `PreviewAction` with
  the currently-selected `Command`. We extend that so that during
  `Filling[i]`, every keystroke in the search box re-raises `PreviewAction`
  with a _resolved-so-far_ version of the command — the same `Command`
  object with the `input` string rewritten to substitute the parameters
  filled so far, including a live substitution of the current slot with the
  user's typed text.
- **Empty values are allowed by default.** If the user hits <kbd>Tab</kbd>
  on an empty slot, that parameter resolves to the empty string. No
  validation pop-up; we trust the user. (Defaults and required-ness are
  follow-up work — see Implementation Plan.)
- **Cancellation is destructive.** Pressing <kbd>Esc</kbd> in `Filling[i]`
  cancels the entire dispatch — we do not send a partially-filled command
  to the shell. This matches every other "we opened a UI you might want to
  back out of" surface in the Terminal.

### Handling duplicate parameter references

Mike was explicit on this one: **if `${host}` appears N times in the
same snippet, the user fills `host` exactly once**, and all N occurrences are
substituted together.

This shakes out naturally from the design above. The set of `parameters`
declared on the snippet is the _ordered_ list the state machine walks
through; each entry corresponds to one slot the user fills. The
substitution step at dispatch time is a simple find-and-replace across the
entire (possibly multi-line) `input` string for each `${<name>}` token.

Example:

```jsonc
{
    "name": "Test, push, and tag a release branch",
    "command":
    {
        "action": "sendInput",
        "input":
        [
            "git checkout -b release/${version}",
            "npm test",
            "git push -u origin release/${version}",
            "git tag v${version}"
        ],
        "parameters":
        [
            { "name": "version", "description": "Semver, e.g. 1.4.0" }
        ]
    }
}
```

`${version}` is referenced four times across three of the four lines.
The user is asked for it _once_. All four occurrences get the same value.

> [!NOTE]
> If a snippet references `${foo}` in its `input` but never declares
> `foo` in its `parameters` array, that's a settings error. We should treat
> it the way we treat other settings errors in `.wt.json` parsing — log a
> warning, skip the snippet, surface it in the snippets pane and via the
> usual Toast. We should _not_ silently send `${foo}` to the shell;
> that's a bug factory.
>
> Symmetric case: if `parameters` declares a name that isn't referenced in
> `input`, that's a warning (not an error) — we'll show the snippet but
> just skip that slot during filling. The most likely cause of this is a
> typo (`{$host}` instead of `${host}`), and a warning is the
> right call-out.

## UI/UX Design

I don't have screenshots yet (this is a spec, not a PR), so here are some
ASCII sketches of what the parameter-filling mode looks like inside the
Suggestions UI.

**State: `Browsing`** — exactly today's behavior, nothing has changed yet.

```
┌────────────────────────────────────────────────────────────────────┐
│ 🔍 ssh                                                             │ ← _searchBox
├────────────────────────────────────────────────────────────────────┤
│ 🖥️  SSH to a dev box                                                │ ← filtered list
│     ssh ${user}@${host}                                             │
│                                                                    │
│ 🖥️  SSH with port forwarding                                        │
│     ssh -L ${local}:localhost:${remote} ${host}                     │
└────────────────────────────────────────────────────────────────────┘

   PS C:\dev> ssh user@host█        ← ghost-text preview in TermControl
```

**Transition: user presses <kbd>Enter</kbd> on the first snippet.** We do
_not_ call the existing `_dispatchCommand` path. Instead we detect that the
selected `Command`'s `SendInputArgs` has a non-empty `parameters` array,
suppress dispatch, and transition to `Filling[0]`.

**State: `Filling[0]` — filling `user`**

```
┌────────────────────────────────────────────────────────────────────┐
│ 🔍 _____________  (placeholder: "Your account on the dev box")     │ ← _searchBox cleared, placeholder = parameter description
├────────────────────────────────────────────────────────────────────┤
│ 🖥️  SSH to a dev box                                                │
│     Parameter 1 of 2: user                                          │ ← single-row "you're filling this" indicator
└────────────────────────────────────────────────────────────────────┘

   PS C:\dev> ssh ▎@host█           ← preview: current slot rendered as caret-marker
```

**User types `zadji`. `PreviewAction` re-fires on every keystroke.**

```
┌────────────────────────────────────────────────────────────────────┐
│ 🔍 zadji                                                           │
├────────────────────────────────────────────────────────────────────┤
│ 🖥️  SSH to a dev box                                                │
│     Parameter 1 of 2: user                                          │
└────────────────────────────────────────────────────────────────────┘

   PS C:\dev> ssh zadji@host█       ← preview updates live
```

**User hits <kbd>Tab</kbd>. Advance to `Filling[1]` — filling `host`.**

```
┌────────────────────────────────────────────────────────────────────┐
│ 🔍 _____________  (placeholder: "Dev box hostname …")              │
├────────────────────────────────────────────────────────────────────┤
│ 🖥️  SSH to a dev box                                                │
│     Parameter 2 of 2: host                                          │
└────────────────────────────────────────────────────────────────────┘

   PS C:\dev> ssh zadji@▎█          ← user value committed, next slot is the caret
```

**User types `dev-box-2026`, hits <kbd>Enter</kbd>. State machine reaches
the end of the parameter list; we fall through to today's
`_dispatchCommand` path with the resolved input string.**

```
   PS C:\dev> ssh zadji@dev-box-2026
   (input sent — Suggestions UI closes)
```

### Touch points in `SuggestionsControl`

Concretely, here's what an implementer (hi, Parker) would touch in
`src/cascadia/TerminalApp/SuggestionsControl.{cpp,h,xaml,idl}`. I'm calling
out the symbols by name so we have a shared map of the work:

- **`_dispatchCommand`** (`SuggestionsControl.cpp:697`) — the gate. Today
  it unconditionally fires `DispatchCommandRequested`. We need an early
  branch: if the selected command is a `sendInput` with a non-empty
  `parameters` array, enter parameter-filling mode instead of dispatching.
- **A new state member** on `SuggestionsControl` — something like
  `std::optional<ParameterFillingState>` with the current snippet, the
  parameter index, and the in-progress filled values. Lives next to
  `_currentNestedCommands` and `_nestedActionStack` in
  `SuggestionsControl.h:70`; mirrors that existing "I am in a substate"
  pattern.
- **`_filterTextChanged`** (`SuggestionsControl.cpp:813`) — today it
  re-runs `_collectFilteredActions()` against the search box text. While
  in parameter-filling mode, it instead just re-raises `PreviewAction`
  with a freshly-resolved `Command` (substituting all already-filled
  parameters plus the live search-box text into the current slot). No
  list filtering happens during parameter-filling.
- **`_selectedCommandChanged`** (`SuggestionsControl.cpp:265`) — the list
  is single-row during parameter-filling and selection is fixed on the
  active snippet's "you're filling this" row. The handler should be a
  no-op when we're in `Filling[i]`.
- **Key handler** (the existing one that consumes <kbd>Enter</kbd>,
  <kbd>Tab</kbd>, <kbd>Esc</kbd> on `_searchBox`) — the bulk of the
  work. During parameter-filling, <kbd>Enter</kbd> and <kbd>Tab</kbd>
  commit the current slot and advance; <kbd>Shift</kbd>+<kbd>Tab</kbd>
  steps back; <kbd>Esc</kbd> cancels the whole dispatch and returns to
  `Browsing` (or closes, matching today's Esc behavior).
- **`PreviewAction`** (`SuggestionsControl.idl:43`, `.h:50`) — the event
  contract doesn't change. We still raise it with a `Command`. The only
  difference is that during parameter-filling we raise it _per keystroke_
  with a `Command` whose `SendInputArgs.Input` has been progressively
  substituted. The TermControl side that consumes `PreviewAction` for
  ghost text already redraws on each fire — no changes needed there.
- **`SearchBoxPlaceholderText`** — already exists and is used for
  changing the placeholder when nested commands are active. We reuse it
  to show the current parameter's `description` as placeholder during
  `Filling[i]`. Pattern matches `SuggestionsControl.cpp:919`.
- **Substitution helper** — a new free function in
  `Microsoft.Terminal.Settings.Model` (next to the existing
  `${profile.name}` expansion in `Command.cpp:546`) that does the
  `${<name>}`-to-value find-and-replace. Same shape as the existing
  profile-name expansion. Because parameter tokens share their bracket
  syntax with the existing well-known tokens (`${profile.name}`,
  `${scheme.name}`, etc.), this helper must coexist with that expansion.
  Two options for Parker to pick between: (1) a single token-parsing pass
  that knows both the well-known tokens and the per-snippet parameter
  list, or (2) a second pass that runs after the existing expansion and
  explicitly skips any token already resolved. I have a mild preference
  for the single-pass version because it's one parser to reason about,
  but either shape works — I'll leave the call to whoever picks this up.
  Lives in the settings model, not in `TerminalApp`, so other consumers
  (e.g. the snippets pane previewer) can call it too.

I am _not_ proposing changes to the `sendInput` action wire format beyond
the new optional `parameters` property. The resolved string still goes
through the existing `SendInputArgs::Input()` path at dispatch time.

## Tenets

<table>

<tr><td><strong>Compatibility</strong></td><td>

The `parameters` property is **optional**. A `sendInput` action without a
`parameters` array behaves exactly as it does today — no substitution
attempted, the `input` string is sent verbatim. This means:

1. Every existing snippet in every existing user's settings keeps working
   without change.
2. A user who never declares `parameters` never has to think about this
   feature. They can write `${foo}` into their `input` string and
   it'll go to the shell as literal text, the same as `${anything.else}`
   does today.

Dropping the `input.` namespace means parameter tokens share their bracket
syntax with the existing well-known tokens (`${profile.name}`,
`${scheme.name}`, etc.). That's a slightly different compatibility shape
than a namespaced syntax would have, so it's worth spelling out the rule:
**substitution only happens for snippets that declare a `parameters` array,
and only for `${<name>}` tokens where `<name>` matches a declared
parameter.** Anything else — an undeclared name, a well-known token that
isn't recognized, a literal `${foo}` in a snippet with no `parameters` —
is passed through to the shell verbatim, exactly the same as today. If a
user today has a literal `${foo}`-shaped string in their `input` and
later adds a `parameters` array that happens to include `foo`, that token
will start being substituted. The escape-with-backslash mechanism above
gives us a clean answer for the (vanishingly rare) case where someone
wants both behaviors in the same snippet.

</td></tr>

<tr><td><strong>Accessibility</strong></td><td>

The parameter-filling experience reuses the existing `_searchBox` TextBox,
which is already a fully-accessible XAML control with screen-reader support.
The "you're filling this parameter" status row needs an
`AutomationProperties.Name` set to something like
"Parameter {position} of {total}: {name} — {description}" so a Narrator user
hears _which_ slot they're currently filling, what it's called, and what
it's for.

We should also raise an automation `LiveRegion` announcement on each slot
transition so Narrator says "Now filling: host. Dev box hostname." when the
state machine advances — otherwise the focus stays on the same TextBox and a
sighted-only design would silently change context.

The Suggestions UI's existing `AutomationPropertiesProvider` machinery
(`SuggestionsControl.cpp:572`) is the right place to plumb this through.

</td></tr>

<tr><td><strong>Sustainability</strong></td><td>

Substitution is a string find-and-replace. The "rerun on every keystroke for
the preview" cost is bounded by the length of the `input` string × the
number of parameters, which for a realistic snippet is single-digit
milliseconds at most. No new background work, no new persistent state on
disk, no new network traffic. Neutral.

</td></tr>

<tr><td><strong>Localization</strong></td><td>

The `description` field on each parameter is user-authored content. For a
snippet a user wrote for themselves, that's the user's problem — same as
the `name` and `description` of the snippet itself today.

For snippets coming from a **fragment extension**, we have the same
unresolved question that the [Snippets] spec already flags: do we accept a
`description: { en-us: "…", pt-br: "…" }` localized-map form? My answer
matches that spec's: leave it as a future consideration, and let v1 ship
with single-string descriptions. When we figure out localized snippet names,
parameter descriptions ride along on the same mechanism.

One Terminal-owned string is new: the placeholder text for the parameter
slot when no `description` is provided (something like "Enter value for
{parameter name}"). That goes in `Resources.resw` like every other UI string.

</td></tr>

<tr><td><a name="security"></a><strong>Security</strong></td><td>

> [!WARNING]
> **Snippet parameters are user input that gets concatenated into shell
> input.** This is the textbook command-injection shape. We need to be
> careful, and more importantly, we need to be clear with users about what
> the threat model is.

Let me be explicit about what this feature does and does not change:

1. **A snippet you author yourself in your own `settings.json` is no
   more dangerous with parameters than without.** You're typing input
   into your own shell, the same as if you typed it directly.
2. **A snippet from a `.wt.json` in a trusted folder is no more
   dangerous than the same snippet without parameters.** The existing
   trust-prompt mechanism from [Snippets] (the "do you trust this folder"
   dialog) is what gates the snippet's _existence_; once you've trusted
   the folder, you've already accepted that arbitrary input may be sent
   to your shell.
3. **A snippet from a fragment extension is no more dangerous than the
   same snippet without parameters.** Fragment extensions can already
   contribute `sendInput` actions today.

What changes: **the user's typed parameter value is concatenated into the
input string and sent to the shell.** If a malicious snippet author writes
`input: "echo Welcome ${name}"` and you type `; rm -rf ~` as your
name, the shell will see `echo Welcome ; rm -rf ~`. **This is not a flaw
in snippet parameters; it is the literal mechanism of how command-line
shells work.** It's the same risk the user accepts every time they paste
text into a terminal — which is why we have a paste-warning dialog for
multi-line and special-char clipboard content already.

Concrete things we should do:

- **Do not implement any "shell escaping" on parameter values.** We have
  no reliable way of knowing the shell, the locale, or the quoting
  context where the value is being substituted. Trying to escape would
  create a false sense of safety and would mangle legitimate inputs. The
  contract is: _what you type is what the shell sees_, the same as if you
  typed it directly.
- **Apply the existing pre-send paste warnings.** If a parameter value
  contains a newline (because the user pasted a multi-line string into
  the slot) or any of the dangerous control characters we already warn on
  for paste, we should run the same warning prompt before dispatch. The
  hook point is in the dispatch path, after substitution.
- **Reinforce the `.wt.json` trust-folder dialog from [Snippets].** A
  parameterized snippet from an untrusted source is no _more_ dangerous
  than a non-parameterized one — but it might _feel_ scarier to users,
  because they're being asked to type into it. The trust-folder dialog
  copy may need a refresh once this lands to make the threat model
  explicit.
- **Engage our security partners** before this ships, the same as we did
  for the original Snippets feature. Specifically I want a review on the
  paste-warning hook integration and on whether we need a separate
  acknowledgment the first time the user fills a parameterized snippet
  from a `.wt.json` source.

</td></tr>

</table>

## Potential Issues

A bunch of edge cases that need a position taken on, in roughly decreasing
order of "this will bite us":

### Parameterized snippets bound to a key

You can bind a `sendInput` action directly to a key today. What happens if
the bound action has `parameters`?

I'm proposing: **the keybinding _opens the Suggestions UI in
`Filling[0]` for that snippet_.** The snippet doesn't dispatch directly,
because we don't have any values to substitute. From the user's
perspective: hitting the key brings up the parameter-filling UI for that
specific snippet, exactly as if they had selected it from the Suggestions
UI list and pressed <kbd>Enter</kbd>.

The alternative — dispatching with empty strings for every parameter — is
strictly worse. It quietly sends `ssh @` to the shell, which is a bug.

### `waitForSuccess` + parameters

The [Snippets] spec added `waitForSuccess` for multi-line snippets, to
let each line wait on the previous one's shell-integration
`FTCS_COMMAND_FINISHED`. Parameters interact with this just fine: we
resolve _all_ parameters once at dispatch time, then the resolved
multi-line input goes into the existing `waitForSuccess` pipeline. The
user fills parameters once, before any line is sent, and the resolved
values are baked into the entire script before the first line goes out.

This matters because the alternative — re-prompting between lines — is
both surprising UX and topologically impossible (we'd be putting the
Suggestions UI back up _after_ a line has been sent to the shell, which
is a context where the user expects to be looking at shell output, not
filling forms).

### String escaping in multi-line snippets

A parameter value can contain a `\r\n`-style escape sequence in the
JSON-source `input` string. Mike was specifically worried about this. The
answer: the JSON parser unescapes the input string at load time, and the
resolved-with-substitution string is sent to the shell exactly as the JSON
literally said. Parameter substitution happens after JSON unescaping but
before shell dispatch, so:

```jsonc
"input": "echo line1${foo}line2"
```

with `foo` set to `\r\n` (literal, not escaped — i.e. the user pressed
Enter in the textbox)… won't happen, because the search box doesn't accept
multi-line input. So in practice, parameter values are guaranteed to be
single-line strings, and the multi-line behavior of the snippet is
entirely controlled by the author's choice of `input` array vs. single
string, exactly as today.

If we ever allow multi-line parameter values (paste!), we re-enter the
paste-warning flow above. I think we're fine.

### Cancellation mid-fill

<kbd>Esc</kbd> during `Filling[i]` cancels the entire dispatch — nothing
goes to the shell. This is the only sane answer. The alternative
(dispatch what you've filled so far) is the "you didn't mean to send `ssh
zadji@`" footgun.

Open question: should <kbd>Esc</kbd> instead return to `Browsing` (with
the list of snippets re-shown)? That would be more in-line with how
nested commands work (`SuggestionsControl.cpp:1063`-ish — Esc pops a
nesting level). I'm tentatively saying full-close is fine because a user
who wanted a different snippet would re-summon the Suggestions UI anyway,
but I'd take a vote on this from anyone who has opinions.

### What does the snippets pane do?

The snippets pane ([Snippets] spec) is a longer-lived UI than the
Suggestions UI. It has a `TreeView` of snippets and a "play" button per
row. Pressing play on a parameterized snippet from the snippets pane
needs to do _something_ — either pop the Suggestions UI in `Filling[0]`
mode for that snippet, or grow its own inline parameter-filling
experience.

For v1, **the snippets pane "play" button pops the Suggestions UI in
`Filling[0]`**. We don't grow a second filling implementation. Future
work can do something native to the pane.

## Implementation Plan

### 🐣 Crawl

* [ ] [#TBD] Settings model: add an optional `parameters: Parameter[]` property
  to `SendInputArgs`. Each `Parameter` has `name: string` (required) and
  `description: string` (optional). Round-trips through JSON, with
  serialization tests.
* [ ] [#TBD] Settings model: add the `${<name>}`-substitution helper
  next to the existing `${profile.name}` substitution in
  `Microsoft.Terminal.Settings.Model::Command.cpp`. The helper must
  coexist with the existing `${profile.name}` / `${scheme.name}`
  expansion — Parker's call whether to fold both into a single
  token-parsing pass or to layer a second pass that skips already-resolved
  tokens. Unit tests for: a single parameter, multiple parameters,
  repeated parameter (one-fill, many-substitution), `\${foo}` escape,
  malformed token (parsed as literal), interaction with `${profile.name}`
  in the same string (both resolve correctly).
* [ ] [#TBD] Settings model: validation step that warns on
  `${<name>}` tokens whose `<name>` isn't declared in `parameters`
  (and isn't one of the known well-known tokens), and on `parameters`
  entries whose `name` isn't referenced.
* [ ] [#TBD] SuggestionsControl: parameter-filling state machine —
  `Browsing` / `Filling[i]` / dispatch. Single-parameter snippets first;
  multi-parameter Tab/Shift+Tab navigation second.
* [ ] [#TBD] SuggestionsControl: live preview during fill via
  `PreviewAction`. No new event contract; just re-raise with the
  resolved-so-far command on each keystroke.
* [ ] [#TBD] Accessibility: per-slot Narrator announcement,
  `AutomationProperties.Name` on the "you're filling this" row.
* [ ] [#TBD] Tests: end-to-end test that a parameterized snippet, when
  invoked, blocks dispatch until the parameters are filled.
* [ ] [#12927] Mark this issue as the parent for Crawl-tier work and link
  this spec from it.

### 🚶 Walk

* [ ] [#TBD] Add `default: string` to the parameter schema. Prefills the
  search box for that slot at the start of `Filling[i]`.
* [ ] [#TBD] Keybindings: bound `sendInput` actions with `parameters`
  open the Suggestions UI in `Filling[0]` rather than dispatching.
* [ ] [#TBD] Snippets pane: "play" button on a parameterized snippet
  pops the Suggestions UI in `Filling[0]`.
* [ ] [#TBD] `.wt.json` parameterized snippets: same substitution path,
  same trust-folder gate. Verify the trust-folder dialog copy still makes
  sense once parameters exist.
* [ ] [#TBD] `wt x-save` interaction: if the user `x-save`s a commandline
  that contains a `${<name>}`-shaped token, prompt them for the
  description rather than just dropping a name-less parameter declaration
  in the settings file. (Optional polish; can defer.)

### 🏃‍♂️ Run

* [ ] [#TBD] **Enum-typed parameters.** Schema gains
  `type: "enum"` and `values: string[]`. The Filling[i] UX swaps the
  free-text TextBox for a picker (we have one of these already in the
  Suggestions UI for nested commands; reuse the same XAML primitive).
  This mirrors VsCode's `pickString` input type.
* [ ] [#TBD] **CLI-driven parameter values.** Schema gains
  `type: "command"`, `command: string`, optional `shell: string`. We
  spawn the command at slot-entry time, parse stdout as a newline-
  separated list, present it as the picker. Mirrors VsCode's `command`
  input type. Note the obvious security implications and the failure
  modes when the CLI hangs or returns a megabyte of output.
* [ ] [#TBD] Per-shell parameter encoding. If we ever learn what shell
  the user is running ([#8639] et al.), we _could_ offer optional
  shell-aware quoting (`type: "string", encoding: "posix-shell-arg"`).
  Calling this out as Run-tier specifically because we'd need to be very
  sure of the shell-detection story before shipping any auto-escaping.

<!--
### 🚀 Sprint

* [ ] **cmdpal IParametersPage delegate.** A 3p extension implementing
  the SDK's `IParametersPage` interface can serve the parameter-filling
  UI for snippets. Some examples of what becomes possible:
    - A git-branch picker extension exposes `IParametersPage` with one
      `ICommandParameterRun` whose `SelectValueCommand` is an
      `IListPage` over `git branch -a` output. Snippet authors point at
      it: `{ "name": "branch", "provider": "git-branch-picker" }`.
    - A file-picker extension provides a parameter whose value is a
      filesystem path with rich preview.
    - A dev-box-list extension reads from a corporate inventory and
      provides a `host` parameter with descriptions and icons per box.
  This is the natural bridge between snippets and the DevPal SDK and is
  worth its own follow-up spec once the SDK is shipped.
* [ ] **Save the filled snippet as a new snippet.** After a fill, offer
  "save this exact invocation as a snippet" — useful when the user
  realizes they're going to be running the same parameterized snippet
  with the same values repeatedly.
-->

## Conclusion

The most common feedback I see on snippets is some variant of "great, but
I had to edit it after it landed". Parameters are the answer to that
feedback. The state machine is small, the syntax rhymes with our existing
substitution tokens, and the surface is small enough to ship behind a
schema bump without touching the wire format of any existing snippet. I
expect this to take a 🐣 Crawl slice to ship and immediately move a
non-trivial number of `.wt.json` files in real users' repos from
"reference card" to "command launcher".

### Future Considerations

* **`IParametersPage` from the DevPal SDK.** The cmdpal SDK already
  specifies `IParametersPage` as the surface a 3p extension uses to
  provide an inline parameter-filling experience (see
  `IParametersPage`, `IStringParameterRun`, `ICommandParameterRun` in
  the [DevPal SDK spec][devpal-iparameterspage]). Today, snippet
  parameters are filled by the Suggestions UI directly. Once the SDK
  ships, we can let snippets _delegate_ their parameter-filling UI to a
  3p extension's `IParametersPage`, unlocking pickers like
  git-branch-from-repo, file-from-tree, hostname-from-tailnet, etc. The
  schema bridge is small — something like `"provider": "extension-id"`
  on a parameter. This is the natural way snippets and cmdpal meet.

* **Enum-typed and CLI-driven parameters.** Listed in 🏃‍♂️ Run above; called
  out here too because they're the two follow-ups I expect to want first
  after Crawl ships. VsCode's `pickString` and `command` input types are
  the precedent; we should match their shape so we get the "users
  already know how this works" benefit.

* **A "required" flag.** Today every parameter is fillable-with-empty.
  Marking a parameter `required: true` would block <kbd>Enter</kbd>
  advancement until the slot has _some_ value. Probably worth doing once
  CLI-driven parameters land, because empty-string-as-CLI-arg is
  meaningful in a way that empty-string-as-textbox-value is rarely.

* **A "secret" / password flag.** VsCode's `promptString` has a
  `password: true`. For snippets that include a token or password slot,
  rendering as a password TextBox (asterisks, no clipboard echo) is a
  cheap and obvious win. Not in v1 because I don't want to encourage
  pasting credentials into snippets; but if users do this anyway, we
  should support it safely.

* **Saving the _filled_ snippet as a new snippet.** Once you've filled
  `host=dev-box-2026, user=zadji` for your SSH snippet eight times in a
  row, you probably want a "save this exact invocation" button. The
  `wt x-save` story is the obvious entry point.

* **Default parameter values per profile.** A profile-local
  `parameterDefaults: { "host": "my-dev-box" }` would let the same
  snippet auto-fill differently in different profiles. Speculative;
  filing it here so we don't forget it.

* **Validation regex per parameter.** A `pattern: "^v[0-9]+\\.[0-9]+\\.[0-9]+$"`
  field would let snippet authors gate dispatch on a regex match. Strictly
  a "we got user feedback asking for this" item, not a v1 ask.

* **Multi-line parameter values.** Today the search box is single-line,
  so parameter values are too. If we ever want a snippet like "send this
  pasted-in stack trace to the AI assistant", we'd need a multi-line
  Filling[i] mode. Re-enters the paste-warning flow from the Security
  tenet.

## Resources

### Footnotes

<a name="footnote-1"></a>[1]: I should call out: the parameter-filling
state machine intentionally does _not_ persist across Suggestions UI
sessions. If you Esc out of `Filling[1]` and re-summon the Suggestions UI,
you start over from `Browsing`. We're not building a save-the-draft
mechanism in v1. The day someone asks for that, they should file an
issue.

<a name="footnote-2"></a>[2]: VsCode's input variable resolution is also a
two-pass process, with one notable rule: "if a variable occurs more than
once, it is only evaluated once" (see [input variables]). Our
deduplication design is in the same spirit, but ours is simpler because we
don't support `command`-typed inputs in v1 — substitution is a pure
string-replace, not a function evaluation, so there's no "expensive side
effect" to worry about double-running. When we get to CLI-driven
parameters in 🏃‍♂️ Run, this rule becomes load-bearing.


[Snippets]: ./Snippets.md
[Suggestions UI]: ./Suggestions-UI.md
[Command Palette]: ../%232046%20-%20Command%20Palette.md
[input variables]: https://code.visualstudio.com/docs/reference/variables-reference#_input-variables
[workflows]: https://github.com/warpdotdev/workflows/blob/main/FORMAT.md
[warp-curl]: https://github.com/warpdotdev/workflows/blob/main/specs/curl/c_url_a_url_and_follow_redirects.yaml
[devpal-iparameterspage]: https://github.com/microsoft/PowerToys/tree/main/doc/devdocs/modules/cmdpal/initial-sdk-spec

[#1595]: https://github.com/microsoft/terminal/issues/1595
[#8639]: https://github.com/microsoft/terminal/issues/8639
[#12927]: https://github.com/microsoft/terminal/issues/12927
[#16185]: https://github.com/microsoft/terminal/pull/16185
[#16513]: https://github.com/microsoft/terminal/pull/16513
