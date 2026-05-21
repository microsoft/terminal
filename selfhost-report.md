# Selfhost build — 2026-05-20

Branch: `dev/cazamor/selfhost/2026-05-20`
Base: `origin/main` @ `8fe6c21ef`

## PRs included

| PR | Title |
|----|-------|
| [#20199](https://github.com/microsoft/terminal/pull/20199) | Introduce Profiles page to SUI |
| [#20200](https://github.com/microsoft/terminal/pull/20200) | Add expander groups to settings UI |
| [#20203](https://github.com/microsoft/terminal/pull/20203) | Redesign new tab menu page → dropdown |
| [#20215](https://github.com/microsoft/terminal/pull/20215) | Update styling of Actions page |
| [#20232](https://github.com/microsoft/terminal/pull/20232) | Port SettingsCard/Expander from WCT and update SUI to use it |
| [#20236](https://github.com/microsoft/terminal/pull/20236) | Use correct page transitions in Settings |
| [#20237](https://github.com/microsoft/terminal/pull/20237) | Replace Bold with SemiBold; clean up new-tab tooltip typography |

## What's new in the Settings UI

- **Profiles** is now its own landing page (#20199). The side-nav drills into per-profile pages instead of expanding inline.
- **Expander groups** organize related rows across pages (#20200). The legacy *Rendering* sub-nav has been folded into the new groups.
- **New tab menu** has been redesigned as a dropdown-style layout (#20203).
- **Actions page** has refreshed styling, including the new `KeyChordVisual` (#20215).
- **SettingsCard / SettingsExpander** controls have been ported from the Windows Community Toolkit (#20232). The legacy `SettingContainer` element has been fully replaced; pages use `<local:SettingsCard>` and `<local:SettingsExpander>` directly.
- **Page transitions** in Settings now slide left/right when navigating into and out of sub-pages, and fade in when switching top-level nav (#20236).
- **Typography**: `Bold` weight has been replaced with `SemiBold` across the app; new tab tooltip typography is tidied up (#20237).

## Suggested test cases

1. **Profiles landing**: Click "Profiles" in the side nav. Verify the landing page shows the profile list (not an expanded inline list).
2. **Profile drill-down**: Click into a profile, then any of Appearance / Terminal / Advanced. Verify the page **slides in from the right**. Click the breadcrumb back to the profile. Verify the page **slides in from the left**.
3. **Top-level nav**: Click between Launch / Interaction / Compatibility / Actions / New Tab Menu / Extensions. Verify each new page **fades in** (no horizontal slide).
4. **Expander groups**: On Launch / Interaction / Compatibility / Profiles_* pages, verify that related rows are grouped under a `SettingsExpander`. Toggle expanded / collapsed; verify the chevron animates and the content height animates.
5. **New tab menu**: Open Settings → New Tab Menu. Verify the redesigned dropdown layout. Add / move / delete entries.
6. **Actions**: Open Settings → Actions. Verify the new key-chord visuals render. Edit a key binding; verify the row uses a `SettingsCard` and the editing flyout still works.
7. **Edit Action**: From Actions, drill into a command. Verify both **Key bindings** and **Additional customizations** expanders render. Key bindings should be expanded by default if the command has any chords.
8. **SettingsExpander grouping**: Anywhere you see an expander group, verify that:
   - The chevron rotates on expand/collapse.
   - All child rows are realized (no missing rows after expanding).
   - Card content respects the page width.
9. **Typography**: Spot-check the new tab dropdown tooltip and the tab strip — text should use `SemiBold`, not `Bold`.
10. **Search**: Use the Settings search box. Verify all `SettingsCard`/`SettingsExpander` rows are indexed and reachable from search results. Activating a result should navigate into the right page and scroll to / focus the right card.

## Known issues

- A handful of `WMC1506` "OneWay binding without notification" warnings on `NewTabMenu.xaml` (pre-existing, related to `FolderTreeViewEntry` properties) — non-fatal, build succeeds.
- Some `SettingContainer`-only behaviors (reset-to-default chevron, per-row "Current value" badges) are not yet replicated on `SettingsCard` / `SettingsExpander`. These were intentionally dropped as part of #20232's migration.

## Build

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
msbuild OpenConsole.slnx /t:"Terminal\CascadiaPackage" /p:Configuration=Debug /p:Platform=x64 /p:AppxBundle=false /p:GenerateAppxPackageOnBuild=false /m
```

Build succeeded on x64 Debug.
