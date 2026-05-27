---
author: Mike Griese
created on: 2026-05-26
last updated: 2026-05-26
issue id: 12927
---

# Snippet Parameters — Inline Fill Addendum (🚶 Walk)

> [!NOTE]
> This is an addendum to [Snippet Parameters], which is itself an addendum to
> [Snippets]. It revises the 🚶 Walk-tier UX **only**. The settings schema,
> `SendInputArgs::Resolve` semantics, the `${name}` substitution rules, the
> escape rules, and Parker's `_filterToSnippets` parameter-preservation fix
> are all still correct as written. Don't re-read those, they're fine.

## The pitch

Crawl was the modal fill. The user picked a snippet, the SuggestionsControl
went into a sub-mode, and we presented a TextBox-in-the-dropdown UI for each
parameter, one at a time, with a description panel underneath. It works.
It's also weird in exactly the ways Brady called out — the thing you're
filling in disappears from the preview until you type, and the surrounding
chrome (the back button in particular) is full of leftover state from the
"I'm browsing snippets" mode that no longer makes sense.

Walk pivots: **the SuggestionsControl gets out of the way entirely once you
pick a parametrized snippet, and the fill happens inline at the cursor as a
live template**. The preview row IS the editor. Tab moves between parameters,
typing fills them, Enter commits to the shell, Esc bails. The dropdown is
gone; in its place we float a small tooltip near the active parameter
showing its name and description.

Think TextMate/VSCode snippets, but rendered through whatever channel makes
sense in a terminal. Which is the open question — see [§3](#what-rendering-channel-does-this-live-in).

## The user experience

Walk-through. User has a snippet defined like this (unchanged from Crawl):

```jsonc
{
    "name": "Checkout a branch",
    "command": {
        "action": "sendInput",
        "input": "git checkout ${branch}",
        "parameters": [
            { "name": "branch", "description": "The branch to check out" }
        ]
    }
}
```

1. User opens the snippet picker (`F11` / command palette / however they get
   there today — unchanged).

2. User filters to "Checkout a branch", hits Enter.

3. **The dropdown closes.** Not "transitions to fill mode" — closes. Gone.

4. At the shell prompt, the user now sees:

   ```text
   PS C:\> git checkout branch
                       ━━━━━━
   ```

   Where `branch` is rendered in some visually-distinct style. I have
   opinions but no decision — call it "the tabstop style" for now and pick
   the actual treatment in implementation. Plausible: inverse video, alt
   color from the theme, underline, the existing snippet-preview style we
   already render (which I think is dimmed-italic? Dallas would know).
   The literal characters of the parameter NAME are the placeholder. This
   directly addresses Brady's first complaint — the user sees what they're
   filling.

5. User types `m`. The preview becomes:

   ```text
   PS C:\> git checkout m
                        ─
   ```

   The placeholder `branch` is replaced character-by-character. The
   user-typed `m` adopts whatever style we use for "the active edit"
   (probably normal preview style — non-distinct, looks like real input).

6. User types `ain`. Preview is now `git checkout main`, all in the
   "user typed this" style. No more placeholder visible.

7. Floating near the cursor (above? to the right? Dallas's call) is a
   tooltip:

   ```
   ┌─────────────────────────────────────┐
   │ branch                              │
   │ The branch to check out             │
   └─────────────────────────────────────┘
   ```

   This is the spiritual successor to `_descriptionsBackdrop`. Same content,
   different placement. Follows the active tabstop as the user Tabs between
   parameters.

8. Tab → if there's a next declared parameter, focus moves to it, its
   placeholder gets the tabstop style, the tooltip updates. For our
   single-parameter example, there is no next — Tab is a no-op or
   equivalent-to-Enter (open question, see [§7.3](#7-edge-cases--open-questions)).

9. Shift+Tab → previous parameter. The text the user already typed for
   the previous slot is preserved; revisiting and editing is allowed.

10. Enter → commit. The fully-resolved string is sent to the shell as if
    the user had typed it (same path `sendInput` uses today, same path
    Crawl uses).

11. Esc → cancel. The inline preview disappears completely. The cursor
    returns to wherever it was, with whatever the user had actually typed
    before the snippet picker opened still intact. **Nothing is sent.**

## What rendering channel does this live in?

This is the load-bearing decision Brady has to make, and the answer
gates Ripley's implementation plan. I'm laying out the options, not
picking. I have a leaning. Ripley should investigate before Brady commits.

### (a) Ghost-text in the renderer's input row

Render the template into the shell-integration-ish ghost-text channel the
renderer already has (or is adjacent to — confirm scope with ControlCore).
Intercept keys at the TerminalControl input layer; never send bytes to the
shell until commit.

**Plausible because:** lives in the same surface the user is looking at,
no z-order math, scrolls with the buffer for free, probably works in
fullscreen TUIs (see [§7.1](#7-edge-cases--open-questions)).

**Blockers:** styling. The renderer has no notion of "this run is the
active tabstop, this run is the placeholder, this run is normal preview".
Adding it is a cell-attribute model change. Also: generalizing the key
interception path to "be a tiny text editor" is non-trivial — we have
some of it for existing suggestions integration but not all of it.

### (b) XAML overlay positioned at the cursor

TerminalApp draws a transparent XAML surface on top of the renderer.
Overlay owns the textbox, the styling, and the tooltip; positions itself
at the current cursor cell. Keys go to the overlay; commit sends a
`sendInput` with the resolved string and dismisses.

**Plausible because:** we already do XAML-on-renderer for the Suggestions
UI, tab tear-out preview, OverviewPane. Styling is trivial (it's XAML).
Tooltip is a `ToolTip` or `Popup` — already a primitive.

**Blockers:** position-sync. Cursor moves, buffer scrolls, window resizes,
overlay has to follow. The data is there (IME composition needs cursor
position), but a continuously-updating overlay is real work. And:
[§7.8](#7-edge-cases--open-questions) — what if the cursor scrolls
off-viewport mid-fill?

### (c) Push the template via shell-integration ghost-text

Emit a sequence that says "show this as suggested completion, here's the
cursor, here's the highlight"; let the shell render it.

**Plausible because:** reuses infrastructure cooperating shells already
have.

**Blockers:** we'd be inventing a richer protocol than anything that
ships today. Coupled to the shell. Only works where the shell cooperates.
Multi-quarter cross-org thing. Hard pass from me unless I'm wrong about
scope.

### (d) Something else

Always include this option. Maybe a hybrid — template lives in the
renderer's ghost-text channel for free positioning, XAML overlay only
does styling/tooltip/key capture. That's just (a)+(b), but I'm open to
a better synthesis.

### My leaning

**(b)**. XAML overlay. The Suggestions UI is already XAML-on-renderer
and the patterns are paved. Styling is free. Tooltip is free. Key
interception is the same pattern the Suggestions UI uses today (we're
already not sending keys to the shell while the dropdown is open).

The position-sync work is real but bounded. (c) is a non-starter for
scope. (a) is interesting but requires renderer changes that I don't
think we want to make for this feature — we're trying to ship a UX,
not extend the cell-attribute model.

But this is a XAML-vs-rendering-surface call that Dallas and the
ControlCore folks should sanity-check. Ripley: please investigate
position-sync feasibility against the existing cursor-position plumbing
before Brady commits.

## What goes away

Crawl built a bunch of UI inside the SuggestionsControl that Walk doesn't
need anymore. Rip it out (or, more politely, gate it behind whatever
"the user is in Crawl-tier fill mode" flag we use during the transition
window — but the long-term answer is: gone):

- **The in-dropdown TextBox repurposed as the parameter input.** The
  `_searchBox`-becomes-the-fill-input mechanism Dallas wired up in the
  Crawl pass. The whole `_enterParameterFilling` / `_updateUIForParameterSlot`
  / `_exitParameterFilling` state machine on SuggestionsControl, gone.
- **The in-dropdown description panel reuse.** `_descriptionsView`'s
  Crawl-tier dual-life as a parameter-description renderer (the
  `_descriptionTitle` + `_descriptionComment` repurposing). The description
  panel goes back to being only-for-command-descriptions in the
  Browsing mode.
- **The parameter-name / "N of M" status chrome.** All of the
  `SuggestionsControl_ParameterStatus*` resw strings, the announcement
  strings. Walk's UI is the inline template; there's no "you are on
  parameter 2 of 5" prose because the user can SEE which parameter is
  active from the highlight.
- **The back button in fill mode.** Brady's second complaint. In Walk,
  the SuggestionsControl is closed during fill — the back button is
  not visible because it's not relevant. (The button itself stays for
  nested-command browsing, which is its real job.)
- **The keyboard handling for Tab/Shift-Tab/Enter inside
  SuggestionsControl during fill.** Moves to the inline overlay (or
  whatever channel we pick).

**`_descriptionsBackdrop` survives in spirit, not in place.** The
content (parameter name + description) is exactly the right content
for the floating tooltip. The XAML element living inside the
SuggestionsControl card almost certainly isn't reusable as-is — the
new home is a `Popup` or `ToolTip` adjacent to the cursor, not a child
of the dropdown card. Dallas, name the new element something close
enough that the lineage is obvious.

The `[SnippetParams]` debug logging from the Crawl decisions ledger
should stay through Walk dev and come out before ship.

## What stays unchanged

Everything below the SuggestionsControl is fine. Don't touch it.

- **`settings.json` schema.** `Parameters` array on `sendInput` actions
  exactly as specced. `name` + `description` fields. Same JSON shape.
- **`SendInputArgs::Resolve(IMap<String, String>)` semantics.** Single-pass,
  skip-on-dot, pass-through on undeclared, declared-but-empty → empty
  string, `\$` and `\\` escapes. Parker's `ActionArgs.cpp` implementation
  is the contract. Walk calls `Resolve` exactly the same way Crawl does.
- **`${name}` token substitution.** Token form, regex, escape rules,
  case sensitivity — all unchanged.
- **The snippet-pipeline parameter-preservation fix** Parker shipped in
  `_filterToSnippets` (`ActionMap.cpp`). That bug was upstream of any UI;
  the fix is permanent.
- **The Crawl-tier model layer.** Settings warnings (`SnippetParameterUndeclared`
  error, `SnippetParameterUnused` warning), the `Parameter` runtimeclass,
  the hand-rolled `SendInputArgs` Equals/Hash/Copy. All correct.

The Walk pivot is a UI-only change. Model and settings ship untouched.

## Tabstop semantics

Formalize, because the inline UX is more visible about ordering than the
modal one was and we'll get bug reports if we hand-wave:

1. **Initial tabstop** is the first entry in the snippet's `parameters`
   array. NOT the first `${name}` token encountered while scanning `input`
   left to right. (These are usually the same. They don't have to be.)
2. **Order** is declaration order. If `parameters` is `[branch, remote]`
   but `input` is `git push ${remote} ${branch}`, the initial tabstop
   is still `branch`, even though `remote` appears first in `input`.
   The user authoring the snippet picks the order; we don't reorder it.
3. **Tab** = forward, **Shift+Tab** = backward. **Open question:** wrap
   or clamp at the ends? See [§7.3](#7-edge-cases--open-questions). I lean wrap because the
   array is small and wrap is more forgiving when the user has already
   committed to ditching the modal "N of M" mental model.
4. **Repeated `${name}`** in `input` mirrors typing. Same as Crawl. The
   user types into the active tabstop and ALL occurrences of the same
   name update simultaneously. Visually: ALL occurrences render with
   the "tabstop active" style — the multi-cursor model from text
   editors that have multi-cursor. Terminal doesn't have multiple
   carets, but it can absolutely have multiple highlighted slots, and
   that's the right shape here. No linked-but-dim mirror variant; just
   N identical Active spans. Type into any of them (well — into the
   one focus is on; the others are read-only echoes), they all update
   together. Resolved by Brady 2026-05-27.
5. **Undeclared `${foo}`** in `input` is NOT a tabstop. It renders
   as the literal text `${foo}` in normal-preview style. Matches the
   Crawl `Resolve` behavior exactly (pass-through). The user can't
   Tab to it, can't edit it, doesn't see a highlight.
6. **Empty parameter value** (user has not typed anything for this
   tabstop yet, including before the first keystroke) renders **the
   parameter NAME** as the placeholder text, in the tabstop-active or
   tabstop-inactive style depending on focus. This is the rule that
   directly fixes Brady's first complaint — the user always sees what
   they're filling in.

## 7. Edge cases / open questions

Numbered so Brady can answer them by number. My take on each, but
these are calls Brady (and in some cases Ripley) should make.

1. **Input row wraps.** Template doesn't fit on the remaining columns.
   Renderer wraps to the next row. For (b), the overlay has to follow —
   needs the buffer width, which the renderer exposes. For (a), free.
   *My take: don't be clever; let wrap happen, draw the active tabstop
   wherever it lands, move the tooltip with it.*

2. **User clicks elsewhere mid-fill.** Another tab, the title bar,
   somewhere weird in the renderer. *My take: click outside the active
   tabstop = cancel (equivalent to Esc). "Click outside to commit" is
   defensible but worse — the user has no visual indication of which
   click means what.*

3. **Tab at the ends — wrap or clamp?** *My take, take two: Tab-on-last
   commits (synonym for Enter), Shift+Tab-on-first is a no-op. This
   contradicts my leaning in §6.3 — I changed my mind while writing
   this section. Sorry. Brady picks.*

4. **PSReadLine / cooked-read ghost text collision.** Shell is already
   drawing ghost text. *My take: for (b), the overlay swallows keys,
   so the shell isn't seeing input, so it isn't producing ghost text.
   Problem solves itself. For (a), this is a real problem and another
   reason I lean (b).*

5. **Zero declared `parameters` but `${foo}` tokens in `input`.**
   Resolver pass-through ships today. We don't enter fill mode (gated
   on `Parameters.Size() > 0`, same as Crawl). *My take: correct. No
   change. Worth a test case.*

6. **Newlines in `input`.** Renderer handles `\n` as a line break;
   template wraps to the next line. *My take: for (b), overlay must
   render multi-line — XAML TextBlock does this fine, per-character
   position math gets fiddlier. Not a blocker. For (a), free.*

7. **`${profile.name}` inline resolution.** Crawl said no — Pass 1
   owns dot-bearing names, by the time `Resolve` runs they've already
   been expanded. *My take for Walk: same answer. The user sees the
   literal expanded value (e.g. "Ubuntu") in the template, no highlight,
   no tabstop. Consistent with Crawl, matches mental model.*

8. **XAML overlay position-sync when the buffer scrolls (option b only).**
   User triggers fill; before they finish, the shell emits output, the
   renderer scrolls, the cursor row index changes. The overlay has to
   follow. *My take: this is the gnarliest problem with (b) and the
   reason Ripley should investigate before Brady commits. The mechanism
   exists (IME composition tracks the cursor) but wiring an overlay to
   follow it under fast output is the class of problem that becomes a
   flicker-or-jank ticket six months later. Worst case: cursor scrolls
   off-viewport, we cancel the fill. Not great UX but bounded.*

9. **Accessibility story.** Crawl's `SuggestionsControl_Parameter*`
   strings are mostly obsolete. Tooltip needs an accessibility name;
   the tabstop highlight is visual-only and Narrator needs an
   alternative. *My take: defer until §3 is decided — wiring is
   very different between (a) and (b). Lambert at implementation time.*

10. **Power-user types a complex value into a tabstop** (e.g. `main && pull`
    into `${branch}`). Today it goes through. Walk preserves that —
    no input validation, `Resolve` substitutes whatever the user typed.
    *My take: fine. Validation belongs in 🏃‍♂️ Run's enum / CLI-driven
    parameters.*

## Implementation Plan delta

Replaces the Walk row in [Snippet Parameters]' Implementation Plan
table. Run and Sprint rows unchanged.

### 🚶 Walk (revised)

- [ ] **Pick a rendering channel.** Ripley investigates (a) and (b)
      against existing cursor-position plumbing and renderer cell-attribute
      model. Brady picks. Block all other Walk work on this.
- [ ] **Tear out Crawl-tier fill UI from `SuggestionsControl`.** Remove
      the `_enterParameterFilling` / `_updateUIForParameterSlot` /
      `_exitParameterFilling` state machine, the `_searchBox`-as-fill-input
      repurposing, the `_descriptionsView` parameter-description dual-life,
      and the parameter-status resw strings.
- [ ] **Implement the inline overlay** (or the renderer-channel equivalent,
      depending on §3 decision). Owns: cursor positioning, key interception,
      tabstop highlight rendering, tooltip, Esc/Enter/Tab/Shift+Tab,
      commit-via-`Resolve`-then-`sendInput`.
- [ ] **Wire the snippet-picker → inline-fill handoff.** Picker closes,
      inline overlay materializes with template visible and first tabstop
      active. No transitional UI state.
- [ ] **Tooltip content + positioning.** Show active parameter `Name` + 
      `Description`; follow active tabstop on Tab/Shift+Tab; dismiss on
      commit/cancel.
- [ ] **Accessibility pass.** Narrator hears tabstop transitions, hears
      committed text, hears tooltip content. (Specifics TBD by channel
      decision — see §7.9.)

## Footnotes / link references

- The Crawl-tier ledger entries (Ripley's Decisions 1–7, Parker's
  `_filterToSnippets` fix, Dallas's `_enterParameterFilling` wiring,
  Lambert's TAEF tests) are the source of truth for everything below
  the UI seam. Don't re-litigate.
- Existing snippet preview style: confirm with Dallas which visual
  treatment we use today for `PreviewAction`-driven ghost text in the
  renderer; the active-tabstop style should be visually distinct from
  it but in the same family.

[Snippet Parameters]: ./%231595%20-%20Suggestions%20UI/Snippet%20Parameters.md
[Snippets]: ./%231595%20-%20Suggestions%20UI/Snippets.md
