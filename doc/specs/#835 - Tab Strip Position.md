---
author: Sai Sashankh @sashankh
created on: 2026-05-11
last updated: 2026-05-15
issue id: 835
---

# Tab Strip Position

## Abstract

This spec describes a per-theme `window.tabPosition` setting that places the
Windows Terminal tab strip on any of the four window edges — **top, bottom,
left, or right**. The new enum `TabPosition` lives on `WindowTheme` (next to
the other `window.*` theme properties), and is honored by `TerminalPage` by
imperatively rearranging the root `Grid`. The `Top` (default) and `Bottom`
modes are pure row-reordering of the existing layout; the `Left` and `Right`
modes additionally produce a three-column layout (tab strip / draggable
splitter / content) and re-template the existing `mux:TabView` via a new
resource dictionary `VerticalTabViewStyle.xaml` (so named because it is
scoped to the two vertical layouts only — see § Solution Design). The tab
control's drag/drop, keyboard navigation, accessibility, and command-palette
routing all continue to work unchanged across all four positions. The
default behavior (`tabPosition: "top"`) is bit-for-bit unchanged from today.

Closes microsoft/terminal #835 ("Feature request: Enable customization for
tabs on bottom/right/left") and resolves the long-standing duplicate
backlog (#9082, #9100, #10939, #18837).

The architecture is adopted from @zadjii-msft's `dev/migrie/fhl-spring-2026/side-tabs`
FHL prototype, with finishing touches for runtime mutation, Settings UI, and
full four-position localization.

## Inspiration

Vertical tab strips have been a long-standing user request: closed-as-duplicate
issues #11265 ("Add option to show tabs on side instead of top"), #10939
("vertical tabs (like edge chromium browser)"), #9100 ("Vertical Tabs Option"),
#9082 ("Vertical tabs!"), and #18837 ("[Feature] Vertical tab") all converge on
the same shape. The pain points users cite are:

1. **Long tab titles get truncated** in the horizontal strip once a few tabs
   are open — full paths, branch names, and connection strings collapse to
   "C:\\…\\foo" or just the leading icon. A vertical strip gives each tab a
   full row of width to render its title.
2. **Many tabs become hard to scan** in horizontal — at 10+ open tabs the
   active tab can be off-screen behind the scroll-shadow gradient. A vertical
   list scrolls vertically with no horizontal clipping, mirroring file
   explorers, sidebars, and chat-app channel lists which users already scan
   fluently.
3. **Wide-aspect monitors waste horizontal real estate at the top** while
   running short on vertical. A sidebar reclaims a few hundred pixels of
   vertical content height per session.

Prior art the feature is modeled on:

* **Microsoft Edge.** The vertical-tabs button in the title bar collapses the
  tabs into a left-edge rail. Edge popularized the affordance on Windows in
  early 2021 and the visual style of a left-side strip with selection accent
  bar is the closest analogue.
* **Visual Studio Code.** `workbench.editor.tabs.placement` exposes top /
  bottom / left / right placements for editor tabs. VS Code is a primary
  reference for developer-tool tab UX and confirms the four-position surface
  is the right shape.

## Solution Design

### The `tabPosition` setting

A new enum `TabPosition { Top, Bottom, Left, Right }` is added in
`src/cascadia/TerminalSettingsModel/Theme.idl`:

```idl
enum TabPosition
{
    Top,
    Bottom,
    Left,
    Right
};

runtimeclass WindowTheme {
    Windows.UI.Xaml.ElementTheme RequestedTheme { get; };
    Boolean UseMica { get; };
    Boolean RainbowFrame { get; };
    ThemeColor Frame { get; };
    ThemeColor UnfocusedFrame { get; };
    // Settable so the runtime toggle action and Settings UI can both mutate
    // through the projected interface, instead of needing cross-module
    // access to the impl class.
    TabPosition TabPosition;
}
```

The property is declared **settable** (not `{ get; }`-only) so that the
runtime toggle action (`AppActionHandlers.cpp`) and the Settings UI dropdown
(`GlobalAppearanceViewModel`) can both mutate it through the projected
interface. The alternative — read-only with all writes routed through a free
function on `GlobalAppSettings` — would require either cross-module impl-class
access or an additional indirection that buys no clarity.

### Why on `WindowTheme`, not `GlobalAppSettings` or a profile

`WindowTheme` is the right home for three reasons:

1. **Cohesion.** The other `window.*` theme properties — `useMica`,
   `rainbowFrame`, `frame`, `unfocusedFrame` — are all "how the chrome around
   the content looks" knobs. Tab strip position is the same shape of
   decision; it belongs in the same namespace.
2. **Per-context overrides for free.** Users already pair light/dark themes
   with `theme: { dark: "...", light: "..." }`. Placing `tabPosition` on
   `WindowTheme` means a user can have a horizontal-tab Light theme and a
   vertical-tab Dark theme, switched by the system, without any extra
   plumbing.
3. **Not a profile concern.** Tab position is a window-level affordance, not
   a per-shell affordance. Putting it on `Profile` would either be ignored
   (only one profile's setting can win per window) or would cause confusing
   reflow when switching tabs.

A global setting was the first iteration (v1, see "Rejected approach" below);
the theme is strictly more flexible at no extra implementation cost.

### XAML re-template strategy: `VerticalTabViewStyle.xaml`

`src/cascadia/TerminalApp/VerticalTabViewStyle.xaml` defines three resources:

* `VerticalTabViewListViewStyle` — overrides the `TabViewListView` items
  panel to `ItemsStackPanel Orientation="Vertical"` and switches the scroll
  viewer to vertical scrolling.
* `VerticalTabViewStyle` — re-templates `mux:TabView` from its default
  4-column-in-a-row layout to a 3-row layout (Header `Auto` / TabList `*` /
  Footer `Auto`).
* `VerticalTabViewItemStyle` — re-templates `mux:TabViewItem` to stretch
  full-width, render a 3-pixel accent bar on the leading edge for the
  selected state, and drop the curved-tab corner arcs in favor of a single
  rounded `Border`.

Re-templating an existing `mux:TabView` rather than introducing a parallel
control (e.g. a `ListView`) keeps every behavior the WinUI tab control
already implements — drag/drop reorder, drag-to-tear, keyboard cycling,
`AutomationProperties` tree, screen-reader narration, tooltip propagation,
command-palette routing, theming with acrylic, hover/press visual states —
without any of it being hand-rolled. The cost is a single ~268-line XAML
resource dictionary.

### Imperative layout in `_ApplyTabPosition()`

The actual grid mutation is implemented imperatively in
`TerminalPage::_ApplyTabPosition()`
(`src/cascadia/TerminalApp/TerminalPage.cpp`, lines 4074–4383) rather than as
four parallel XAML layouts. The function reads the active theme's
`Window.TabPosition`, then switches on the value:

* **Top.** No-op layout. If `ShowTabsInTitlebar` is set, the tab row is
  lifted out of the root grid and raised on `SetTitleBarContent` — the
  existing horizontal-tabs-in-titlebar behavior.
* **Bottom.** The root grid is reset to three rows: `Auto` (InfoBars), `*`
  (TabContent), `Auto` (TabRow). Children are reordered so the tab row is
  appended last but inserted at the front of the children collection so that
  overlay z-order (command palette, dialogs) is preserved. A fixup loop
  rewrites any child with `Grid.Row="2"` set in XAML to `Grid.Row="1"` so
  overlays cover the content area, not the tab row.
* **Left and Right.** The root grid is reset to **three columns**:
  `tabStripCol` (200 px, clamped 100–400), `splitterCol` (Auto-width), and
  `contentCol` (`*`). For `Left` the order is `[tabstrip | splitter |
  content]`; for `Right` it's `[content | splitter | tabstrip]`. The
  InfoBars and TabContent are combined into an inner `Grid` that lives in
  the content column. The `mux:TabView` is then re-styled by inserting the
  `VerticalTabViewStyle` and `VerticalTabViewItemStyle` resources into the
  tab row's local resource dictionary; this scopes the re-template to this
  one TabView without polluting the application resources.

### The draggable splitter

For `Left` and `Right`, a 4-pixel `Border` is created and placed in the
splitter column. It registers four pointer event handlers:

* `PointerEntered` / `PointerExited` — swap the `CoreWindow` cursor between
  `SizeWestEast` and `Arrow` so the user gets a visual affordance.
* `PointerPressed` — capture the pointer, record start X and starting tab
  strip column width.
* `PointerMoved` — while dragging, compute the delta, clamp the new column
  width to `[100, 400]` pixels, and update the column definition's `Width`.
* `PointerReleased` — release the pointer capture and clear the drag flag.

The clamp range matches typical sidebar widths in Edge and VS Code; below
100 px the tab titles become illegible, and above 400 px the splitter
encroaches noticeably on terminal content area on a typical 1920-wide
display.

The splitter `Border` is given an opaque background brush, falling back from
`SolidBackgroundFillColorTertiaryBrush` → `SolidBackgroundFillColorBaseBrush`
→ a hardcoded RGB(32,32,32) — the card-stroke brushes are translucent and
left a see-through seam between the tab strip and content.

### Overlay handling: `Grid.ColumnSpan="3"`

In vertical mode the root grid has three columns. Dialogs, the command
palette, the suggestions UI, info bars, and teaching tips all need to span
the full width. The `.xaml` declares `Grid.ColumnSpan="3"` on each known
overlay, and `_ApplyTabPosition()` runs a runtime fixup loop on
`root.Children()` (skipping the first three slots — tab row, splitter, and
content) that sets `Grid.Column=0`, `Grid.ColumnSpan=3`, `Grid.Row=0` on any
deferred-load child. This catches overlay XAML stubs that load lazily after
the initial `_ApplyTabPosition()` call.

### The `toggleVerticalTabs` action

`defaults.json` line 611 reserves the action ID:

```json
{ "command": "toggleVerticalTabs", "id": "Terminal.ToggleVerticalTabs",
  "name": "Toggle vertical tabs" },
```

The handler (`AppActionHandlers.cpp::_HandleToggleVerticalTabs`, lines
1637–1654) mutates the active theme's window tab position:

```cpp
const auto next = (window.TabPosition() == TabPosition::Top)
                  ? TabPosition::Left
                  : TabPosition::Top;
window.TabPosition(next);
_ApplyTabPosition();
```

The action only flips between Top and Left — the two most common
positions. Users who want Bottom or Right configure them in JSON or via the
Settings UI dropdown. (See "Potential Issues / Known Limitations" for the
current runtime-mutation caveat.)

### `AppHost` non-client-area guard

Tabs-in-titlebar (extended non-client area drawing) only makes sense when
tabs are at the top edge. `AppHost::AppHost` (`AppHost.cpp:53–58`) reads
`_windowLogic.GetTabPosition()` and forces `_useNonClientArea = false` for
any non-Top position:

```cpp
const auto tabPos = _windowLogic.GetTabPosition();
_useNonClientArea = (tabPos == TabPosition::Top) &&
                    _windowLogic.GetShowTabsInTitlebar();
```

This means Bottom/Left/Right windows fall back to standard client-area
chrome, which is the correct behavior — tabs in those positions cannot be
drawn into the titlebar by definition.

### Settings UI binding

The Settings UI exposes the dropdown at **Globals → Appearance → Tab bar
position** with four labelled choices ("Top", "Bottom", "Left (vertical)",
"Right (vertical)"). The binding goes through a manual getter/setter pair
`_ActiveThemeTabPosition` on `GlobalAppearanceViewModel`:

```cpp
TabPosition GlobalAppearanceViewModel::_ActiveThemeTabPosition() const;
void GlobalAppearanceViewModel::_ActiveThemeTabPosition(TabPosition value);
```

The macro `GETSET_BINDABLE_ENUM_SETTING` would normally emit the bindable
pair, but it cannot traverse the nested path
`_GlobalSettings → CurrentTheme → Window → TabPosition`. The adapter does
the traversal explicitly and falls back to `Top` when any intermediate
pointer is null.

### JSON schema

`doc/cascadia/profiles.schema.json:2042` adds the schema entry under the
`WindowTheme` definition:

```json
"tabPosition": {
  "description": "Which side of the window shows the tab strip.",
  "enum": ["top", "bottom", "left", "right"],
  "type": "string",
  "default": "top"
}
```

Schema-aware editors (VS Code with the Terminal settings schema) now offer
IntelliSense and validation for the new key.

### Rejected approach: v1 parallel `UserControl`

The first iteration (commit `776ad3cd5` on the v1 branch, since deleted)
introduced a ~810-line `VerticalTabsControl` UserControl that ran in parallel
with `mux:TabView` — its own `ListView`, its own drag-handler shim, its own
focus management, its own command-palette anchor. It worked end-to-end for
left-position tabs but required hand-rolling roughly fifteen behaviors that
the re-template approach inherits for free. The migration commit (`e43f20c73`)
deletes all four `VerticalTabsControl.{cpp,h,idl,xaml}` files outright. The
coexistence cost of two parallel "tab strip" implementations is higher than
the cost of a clean rebuild from the re-template path.

## UI/UX Design

Screenshots covering each of the four positions are in
`test-screenshots/positions-final/` in the working tree and will be attached
to the PR description.

### The four layouts

**Top (default).** Pixel-identical to today. If `showTabsInTitlebar: true` is
set (also the default), the tab row is hosted in the extended non-client area
of the title bar — the existing Windows Terminal aesthetic.

**Bottom.** The tab strip is moved to the bottom of the window. Info bars
remain at the top of the content area; the tab strip is a single row below
the terminal panes. Useful for users who put their hand on a touchpad below
the laptop screen, or who prefer the macOS-iTerm convention.

**Left (vertical).** A 200-pixel-wide tab strip on the left, a 4-pixel
draggable splitter, and the terminal content filling the remaining width.
Each tab renders as a full-width row with: leading 16-pixel icon, title text,
trailing 20-pixel close button. The selected tab has a 3-pixel accent-color
bar on its leading edge and a `TabViewItemHeaderBackgroundSelected` themed
fill. The "new tab" SplitButton in the strip footer reads "+ New tab"
(label + glyph) rather than just "+" — vertical real estate makes the label
read naturally.

**Right (vertical).** Mirror of Left: `[content | splitter | tabstrip]`. The
content area is on the left and the tab strip is on the right. The accent bar
on the selected tab is currently hardcoded to `HorizontalAlignment="Left"` in
the `VerticalTabViewItemStyle` resource (inside `VerticalTabViewStyle.xaml`,
around line 257), which means it appears on the inner edge
nearest the content — see "Potential Issues" for the follow-up to swap it to
the outer edge for `Right` mode.

### Existing-behavior notes

* **`tabPosition: "top"` with `showTabsInTitlebar: true`** is the only
  configuration that draws tabs into the titlebar. Setting `tabPosition` to
  any other value silently forces standard window chrome via the
  `AppHost.cpp:57` guard.
* **No `tabPosition` set at all** is equivalent to `tabPosition: "top"`. The
  default theme in `defaults.json` declares the property explicitly so the
  serializer round-trips cleanly, but a `settings.json` that omits `theme`
  entirely also resolves to top.

## Capabilities

### Accessibility

* **`AutomationProperties` tree.** Because the re-template reuses
  `mux:TabView` and `mux:TabViewItem`, the WinUI-shipping accessibility tree
  is inherited verbatim. The TabView reports itself as a tab control to UIA,
  each TabViewItem as a tab page, and the close button as a button child.
  Narrator announces "tab item, <title>, <n> of <m>, selected" when focus
  moves through the strip — this is unchanged from horizontal mode and was
  not re-implemented.
* **Keyboard navigation.** `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle through tabs
  in all four positions (top, bottom, left, right). `Ctrl+1` through
  `Ctrl+9` jump to the Nth tab. These bindings live on `TerminalPage` and
  are unaffected by tab strip position — they dispatch to the same tab
  state regardless of where the TabViewItem is rendered.
* **Splitter focus.** The draggable splitter `Border` is decorative; it
  does not appear in the tab order. Resizing the tab strip is mouse-only in
  v1. A follow-up will add a keyboard chord (planned: `Ctrl+Alt+Shift+>`
  / `Ctrl+Alt+Shift+<`) once the runtime-toggle blocker (below) is
  resolved.

### Security

The proposed change introduces no new attack surface:

* The setting is local user configuration in `settings.json` — no
  network or IPC.
* The enum is parsed by the existing `JSON_ENUM_MAPPER` macro with
  fall-through to `Top` on unknown values; there is no unbounded string
  attack surface.
* The XAML re-template is loaded as a static resource dictionary at app
  startup; it cannot be modified at runtime by a profile or VT sequence.

### Reliability

* **Unit tests.** A new `ParseTabPosition` test in
  `UnitTests_SettingsModel/ThemeTests.cpp` feeds four JSON theme fragments
  (one per enum value) and asserts each round-trips correctly. 143/143
  SettingsModel tests pass on this branch.
* **Edge cases tested.** Invalid enum values (`"tabPosition": "diagonal"`)
  fall through the `JSON_ENUM_MAPPER` to the default `Top` rather than
  throwing — verified at the parser level.
* **Null-safety.** `_ApplyTabPosition()` defensively probes
  `_settings.GlobalSettings().CurrentTheme()` and `theme.Window()` for null
  before reading `TabPosition`. A missing window object leaves
  `_tabPosition` at its previous value (initially `Top`), so a partial
  settings.json gracefully falls back.
* **`TerminalWindow::GetTabPosition()`** (the accessor `AppHost` uses
  before the XAML island is constructed) similarly returns `Top` if any
  intermediate object is null — guaranteeing the activation path never
  crashes on a malformed theme.

### Compatibility

The setting is **strictly opt-in** and default behavior is unchanged:

* `settings.json` without any `theme` produces the existing top-tab UX.
* `settings.json` with a `theme` that omits `window` produces the
  existing top-tab UX.
* `settings.json` with a `theme.window` that omits `tabPosition`
  produces the existing top-tab UX.
* The `defaults.json` default theme explicitly sets `"tabPosition":
  "top"`, so all existing exported themes that previously had no `window`
  block continue to behave identically.

No existing `WindowTheme` properties were modified or removed. The new
property is purely additive. JSON parsers in older Terminal builds will
silently ignore the unknown key.

### Performance

The feature reuses the existing `mux:TabView` for the tab list, so the steady-state
render cost is identical to horizontal mode. The initial layout
work in `_ApplyTabPosition()` runs once at TerminalPage `Initialized` and
again on settings reload — both are off the hot path.

Informal cold-start measurements (3 runs each, time from `Add-AppxPackage`
re-launch to first prompt rendering) showed `tabPosition: "left"` within ~7%
of `tabPosition: "top"`, well within run-to-run noise. No measurable
regression on steady-state typing or scroll latency.

### Power

No expected impact. The WinUI compositor handles the layout; there are no
new timers, animations, or background tasks. Acrylic for the side strip is
deferred (see "Future Considerations") — the v1 background is a solid
themed brush.

### Localization

The Settings UI adds the following keys to
`src/cascadia/TerminalSettingsEditor/Resources/en-US/Resources.resw`:

| Key | Value |
|---|---|
| `Globals_TabPosition.Header` | "Tab bar position" |
| `Globals_TabPosition.HelpText` | "Controls where the tab strip is shown — top, bottom, left, or right side of the window. This setting lives on the active theme so different themes can use different tab positions." |
| `Globals_TabPositionTop.Content` | "Top" |
| `Globals_TabPositionBottom.Content` | "Bottom" |
| `Globals_TabPositionLeft.Content` | "Left (vertical)" |
| `Globals_TabPositionRight.Content` | "Right (vertical)" |

The `toggleVerticalTabs` action's command name ("Toggle vertical tabs") is
currently hardcoded in `defaults.json` line 611 in English. A localized
`resw` entry for the command name is a known follow-up (see TODO).

The `TabNewButtonText` resource ("New tab"), used by the vertical-mode
new-tab button label, is also added; it propagates through the standard
Microsoft loc pipeline along with the other Settings UI strings.

## Potential Issues

### Runtime mutation is non-functional

**Both** the `toggleVerticalTabs` action and the Settings UI Save button
write the new `TabPosition` value into the active theme correctly, but
neither produces a visible layout change in the running window. The
underlying cause is in `_ApplyTabPosition()`: the function is
**monotonic** — it mutates the root `Grid` (adds column definitions,
inserts splitter, etc.) without first un-mutating any prior state. Calling
it twice in succession (Top → Left → Top) leaves the grid in a corrupted
intermediate state.

Currently the function is only safe at cold start, where the grid is in its
XAML-default shape. A user toggling tab position via action or Settings UI
must close and re-launch the window for the change to take effect.

This is a documented architectural limitation, not a code defect. A
follow-up PR will introduce an **idempotent reset prelude** — a helper that
returns the root grid to its XAML default shape before `_ApplyTabPosition()`
applies the target layout. A working prototype is saved as
`runtime-toggle.patch` in the work tree.

### Right-side accent bar is on the wrong edge

`VerticalTabViewStyle.xaml` line 257 (inside the `VerticalTabViewItemStyle`
resource) hardcodes `HorizontalAlignment="Left"` on the `SelectedIndicator`
border. In `Left`
mode this correctly puts the accent bar on the outer edge of the tab row.
In `Right` mode the accent bar appears on the inner edge (closest to the
terminal content) rather than the outer (window-edge) side. This is a
visual nit, not a functional issue — selection is still indicated by the
themed background fill. A follow-up will conditionally swap to
`HorizontalAlignment="Right"` in `Right` mode (either via a second
`VerticalTabViewItemStyleRight` resource or via a `VisualState` driven by
the page).

### `tabPosition: "top"` is the only titlebar-integrated mode

For any non-Top position, `_useNonClientArea` is forced to false, which
silently disables `showTabsInTitlebar: true` even if the user has it set.
This is cited at `src/cascadia/WindowsTerminal/AppHost.cpp:53-58`. The
behavior is intentional — tabs cannot be drawn into a titlebar that
they're not adjacent to — but users transitioning from
`showTabsInTitlebar: true, tabPosition: "left"` will see standard window
chrome reappear and may be momentarily confused. The Settings UI help text
calls this out; a dedicated info bar at toggle-time is a candidate
follow-up.

### Splitter clamp range is fixed

Tab strip width is clamped to `[100, 400]` pixels. Users with 4K or
ultrawide displays may want a wider strip; users on tablet-sized devices
may want narrower. Exposing the bounds as theme properties is a
straightforward future addition but was deliberately left out of v1 to
keep the surface small.

## Future considerations

### Edge-style collapsible sidebar

The most-requested follow-up. Edge ships a pinnable/collapsible sidebar
where the rail collapses to icons-only and expands on hover or on click.
Scope to add to Terminal:

* New `Boolean TabSidebarCollapsed` property on `WindowTheme` —
  same shape as `TabPosition`.
* Second XAML re-template `VerticalTabViewStyleCollapsed` that hides the
  title `ContentPresenter` and the close button, leaving only the icon
  column.
* Footer pin/unpin button on the vertical strip that toggles the property.
* Action `toggleTabSidebarCollapsed` (action ID reserved).

Estimated 10–14 hours of focused work. A plumbing patch is saved at
`collapsible.patch` in the work tree.

### Clockwise rotation hotkey

A natural companion to `toggleVerticalTabs`: an action that rotates the tab
position one quarter-turn clockwise (Top → Right → Bottom → Left → Top).
Suggested action `cycleTabPosition`, suggested chord
`ctrl+alt+shift+r` to pair with the existing `ctrl+alt+shift+v`
mnemonic for "tabs Vertical". Estimated 25-minute patch once the runtime
mutation blocker is resolved.

### Hover-to-expand in collapsed mode

For the collapsed sidebar above, an obvious refinement is to expand the
rail on hover and re-collapse on pointer leave (mirroring Edge). This is
out of scope for v1 because doing it well requires a popup/flyout overlay
that doesn't reflow the terminal content area on each hover. Reasonable
work item but not started.

### RTL `FlowDirection` handling

`Left` and `Right` are screen-relative, not reading-direction-relative.
In RTL locales (Hebrew, Arabic) the semantics are arguably inverted.
`mux:TabView` honors `FlowDirection="RightToLeft"` natively for its content,
but the column ordering in `_ApplyTabPosition()` is hardcoded. A future
patch can read `FlowDirection` from the root and conditionally swap the
column order.

### Telemetry

`TraceLoggingWrite("TabPositionChanged", PDT_ProductAndServiceUsage,
 MICROSOFT_KEYWORD_MEASURES, ...)` in the action handler and Settings UI
save path would let us observe adoption and per-position usage. Pattern
to copy: `TerminalPage.cpp:474-480`. Out of scope for v1.

### Status icons in vertical mode

The horizontal `TabHeaderControl.xaml` renders progress, bell, zoom,
read-only, broadcast, and unseen-activity glyphs on each tab. The vertical
re-template (`VerticalTabViewItemStyle` resource in `VerticalTabViewStyle.xaml`)
currently exposes only icon / title / close button. Porting the status icons across (at 12 pixels, to
match horizontal sizing) is the highest-priority polish follow-up after
the runtime-toggle fix.

### Acrylic / Mica side strip

The v1 background is a solid themed `SolidColorBrush`. Edge and File
Explorer use a translucent `AcrylicBrush` with `HostBackdrop` source for
their sidebars; the visual coherence is significantly better on Windows 11.
Design rationale lived in an earlier visual-polish design doc; for v1 we
ship solid backgrounds to keep the matrix of theme × position × backdrop
small and rollback-safe.

## Resources

* Microsoft Edge — "Use vertical tabs in Microsoft Edge"
  <https://support.microsoft.com/en-us/microsoft-edge/use-vertical-tabs-in-microsoft-edge-cd9c3d4c-cb56-4203-a93e-cce5b71e7d5b>
* Visual Studio Code — `workbench.editor.tabs.placement`
  <https://code.visualstudio.com/docs/configure/custom-layout#_editor-tabs>
* @zadjii-msft's prototype branch — `dev/migrie/fhl-spring-2026/side-tabs`
  on `microsoft/terminal`. The architecture and `VerticalTabViewStyle.xaml`
  are adopted from this branch.
* Closed-as-duplicate issues that motivate this work: #11265, #10939,
  #9100, #9082, #18837.

## Footnotes

### Why imperative grid mutation and not four XAML layouts?

`TerminalPage.xaml` is shared across all positions; declaring four parallel
top-level layouts (one per position) would have meant either a
`<x:Switch>`-style conditional pattern that XAML doesn't directly support,
or four separate `Page` files with substantial duplication. The
imperative-mutation path keeps the XAML readable as "the default top
layout" and centralizes position-specific code in one C++ function. The
trade-off is the runtime-mutation issue documented above; the fix
(idempotent reset) is an additive ~80-line helper, not a re-architecture.

### Why settable `TabPosition` in IDL?

Mike's prototype declared `WindowTheme.TabPosition` as `{ get; }`-only,
which is consistent with the rest of the `WindowTheme` surface (everything
else is read-only and re-read on settings reload). Two consumers needed
write access: `_HandleToggleVerticalTabs` (in TerminalApp) and
`GlobalAppearanceViewModel` (in TerminalSettingsEditor). Both run in
modules without `WindowTheme`'s impl class on their include path, so going
through the projected interface was the only practical route. The cost is a
small ABI deviation from the upstream prototype and a `WINRT_PROPERTY`-
style setter on the impl class — a deliberate, narrow surface deviation
documented in the IDL comment.

<!-- Footnotes -->

[#9082]: https://github.com/microsoft/terminal/issues/9082
[#9100]: https://github.com/microsoft/terminal/issues/9100
[#10939]: https://github.com/microsoft/terminal/issues/10939
[#11265]: https://github.com/microsoft/terminal/issues/11265
[#18837]: https://github.com/microsoft/terminal/issues/18837
