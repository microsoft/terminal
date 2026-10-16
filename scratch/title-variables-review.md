# Runtime Title Variables Review Todos

Date: 2026-09-21
Scope: baseline `7c92ecd03` through `2122d5c61`, including `15283b2fa` and
`c4f709840`. Both reviewers found a clean working tree.
Reviewers: GPT-6 Astra and Claude Opus 5, independent read-only reviews.
No builds, test execution, or runtime reproductions were performed.

This is a local AI-assisted review. Public use requires human verification and
AI-use disclosure under AGENTS.md. Severity below is the aggregate assessment,
not an endorsement of every reviewer severity claim.

## Fix Pass Status

Checked items below mean source changes or explicit design decisions are complete,
not that native tests passed. Editor diagnostics and CRLF-aware whitespace checks
passed. No CLI build, native test execution, or application verification was run.

- R01: reuse the group's existing last-focused-content path; group labels and
  counts no longer depend on profile availability. Added MovedGroupTitleFormat
  with distinct profiles and destination caption checks (unrun).
- R02/R06: inspect inherited invalid-type flags and emit one generic warning.
  Added new-profile/updates fragment loader tests and mixed-error deduplication
  coverage (unrun).
- R03/R05: handle extended UNC, extended drives, rooted and drive-relative paths;
  preserve POSIX backslashes. Added DirectoryNames cases (unrun).
- R04: user removed processName from supported scope. Removed the token constant,
  allow-list entry and supported-variable documentation. The original external
  spec retains it only as a future consideration, not a reserved token.
- R07: added TerminalOutputTitleUpdates using the existing raw-output mock.
  OSC title/directory changes traverse the real control and tab throttle with
  fixed tab text. Assertions cover final-event coalescing, duplicate suppression,
  inactive tabs, connection replacement and stale old-connection output (unrun).
  The existing queued workspace-name test remains.
- R08: added TransferredTitleCallbacks with two differently named pages sharing
  a ContentManager. It uses BuildStartupActions, detach, removal and AttachContent,
  with callbacks pending at transfer/close, output during detach and after close,
  and source-page notification isolation (unrun). Both pages share the UI test
  host's dispatcher: separate-window/dispatcher coverage remains a manual gate.
  Pane::Closed is raised by Close and the null-content branch of _CloseChild;
  detach revokes forwarding while preserving the core. No speculative ownership
  or dispatcher changes were made.
- R09: added desktop ConptyTitleTests for shell/PID, case-insensitive environment
  lookup, base/override values, WT variables, WSLENV, child-side mutation versus
  snapshot immutability, independent connections and metadata after close.
  Added packaged FailedConnectionTitleMetadata; R07 covers replacement callbacks.
  All are unrun. Live inbound handoff still requires the default-terminal
  integration setup; no registration/default-terminal preferences were changed.
- R10: audited ModelLib and added its missing direct TerminalConnection project
  reference with Private=true and ReferenceOutputAssembly=false, retaining the
  explicit WinMD reference. XML and its 19-project reachable graph passed static
  validation (no cycles/missing paths). Normal VS incremental Build, projection/
  object timestamps, launch and package payload checks remain required.
- R11/R14/R18/R20: added independent pane-title expectations, empty/invalid
  fallback, invalid global separator, rejected boxed-type/no-mutation,
  persisted-null re-enable and literal-dollar tests (all unrun).
- R12: removed unused dependency storage/accessor and updated the external spec.
- R13/R15/R16: retain eager parsing, existing workspace-name ownership and separate
  base/final environment snapshots. No evidence justifies changing these designs.
- R17: query PID before locking; token queries removed with connection elevation.
  No contention/performance claim is made.
- R19: user selected Terminal-host-only elevation. Both title surfaces query the
  host helper; no child, WSL or remote privilege tracking. Added shield assertions
  (unrun); elevated/non-elevated application runs still required.
- R21: confirmed and repaired the missing final newline.

## Remaining Execution Gates

All newly added native tests need compilation and execution in Visual Studio.
Editor diagnostics do not validate C++/WinRT ABI, linking, deployment or timing.

1. Build LocalTests_TerminalApp and TestHostApp in VS. Run
  TabTests::TerminalOutputTitleUpdates, TransferredTitleCallbacks,
  FailedConnectionTitleMetadata, QueuedWindowTitleUpdates and
  MovedGroupTitleFormat using the existing packaged UI test host.
  The throttle tests use a one-second dispatcher settling interval with a
  five-second bounded wait; inspect failures rather than treating a timeout
  as success.
2. Run ConptyTitleTests::LaunchMetadataSnapshot and
  IndependentConnectionSnapshots as desktop tests (not RunAs=UAP) from the
  TestHostApp deployment directory. They load the deployed TerminalConnection.dll
  factory directly and launch only cmd.exe /d /q, with cleanup guards.
  They need the build's ConPTY deployment alongside the DLL. They use synthetic
  TITLE_TEST_* values and do not print the host environment.
3. R08: in two actual Terminal windows with different workspace names, use
  `${workspaceName}:${paneTitle}:${cwd}`, rename a tab literally, and change its
  OSC title/directory repeatedly while moving it between windows. Verify only
  the destination caption follows it; close it with output pending and check
  for assertions, stale notifications or crashes. The two-page test cannot
  establish cross-dispatcher correctness or full window teardown.
4. R09 handoff: with this build already selected by the tester as the default
  terminal, launch a fresh console client externally. Check shellName and
  shellPid against the actual launch client, env:TITLE_TEST_* remains empty
  (handoff does not provide a launch environment snapshot), and elevation still
  reflects the Terminal host. Close/move the handed-off tab and verify no stale
  metadata reaches a replacement connection. Do not change OS registration or
  default-terminal selection automatically to run this check.
5. R10: after each relevant IDL producer changes, use normal VS Build (not Clean/
  Rebuild). Confirm the adapter and ModelLib consumer projections/objects rebuild,
  launch the app, and inspect the package for duplicate WinMD/XBF payloads.
  Static project references alone do not prove the original ABI fault is fixed.

## Source-Backed Fixes

- [x] **R01 / P2: Preserve title context when moving a selected pane group to a new tab.**
  Astra 1. `src/cascadia/TerminalApp/Tab.cpp:35`, `:554`, `:588`;
  `src/cascadia/TerminalApp/TerminalPage.cpp:2689`.
  The constructor seeds MRU only for an active leaf. A transferred group can
  leave MRU empty, causing `_GetTitleProfile()` to return null. The resolver's
  early return also loses group title and whole-tab counts. Preserve/reconstruct
  the group's last-focused leaf and resolve group-wide variables independently
  of profile availability. Test a transferred group with differing profiles,
  before selecting a leaf, including its window caption.

- [x] **R02 / P3: Preserve invalid-type warnings from inherited fragment profiles.**
  Astra 2. `src/cascadia/TerminalSettingsModel/CascadiaSettings.cpp:484`;
  `Profile.h:129`; `CascadiaSettingsSerialization.cpp:1116` in the same directory.
  Invalid title-setting types set a local flag on the fragment parent, but
  validation reads the user child's local flag. Aggregate diagnostics across
  inheritance without repeated reporting. Test new-profile and updates fragments
  with numeric formats and null separators through SettingsLoader.

- [x] **R03 / P3: Preserve extended UNC roots in cwdFolder.**
  Astra 3; related Opus F6. `src/cascadia/inc/TitleFormat.h:109`.
  `\\?\UNC\server\share\` is interpreted as an ordinary UNC path with the wrong
  root boundary, returning `share`. Handle extended UNC before ordinary UNC.
  Test roots with/without trailing separators and descendant directories;
  retain POSIX backslash behavior.

- [x] **R04 / Scope Decision: Remove processName support.**
  Opus F3; acknowledged by Astra. `src/cascadia/inc/TitleFormat.h:71`;
  `src/cascadia/TerminalConnection/ConptyConnection.cpp:687`.
  User decision: remove the unsupported token and shipping requirement. It is
  now an unknown variable, not a recognized-but-empty provider. Keep only a future
  consideration in the original spec.

## Reproduction And Verification Todos

- [x] **R05 / P2 Candidate: Test rooted and drive-relative cwdFolder inputs.**
  Opus F6. `src/cascadia/inc/TitleFormat.h:100`.
  `\foo\bar` returns the whole path; `C:foo` retains the drive prefix.
  Determine supported/reachable input forms before changing classification.
  Test those forms and extended drive paths. Do not classify every backslash as
  Windows syntax: backslashes can be legal POSIX filename characters.

- [x] **R06 / P3 Candidate: Verify warning multiplicity and define deduplication.**
  Opus F7. `src/cascadia/TerminalSettingsModel/CascadiaSettings.cpp:467`.
  Several branches append the same enum. Reproduce with multiple invalid profile
  types, inherited defaults, shared malformed templates, and invalid globals.
  Do not assume a defaults flag is copied into every child. Decide whether the
  UI should show one generic warning or one per distinct offending template.

- [x] **R07 / P2 Verification: Add actual asynchronous window-title event tests.**
  Astra runtime gaps; Opus test gaps. `src/cascadia/TerminalApp/TerminalPage.cpp`
  (`_ScheduleTitleUpdate`); `LocalTests_TerminalApp/TabTests.cpp:702`.
  Existing direct calls to Title() do not validate event delivery. Hold tab text
  constant/renamed, update cwd or pane title, pump the dispatcher, and assert final
  caption, coalescing, deduplication, and no updates from inactive-tab context.

- [ ] **R08 / P2 Verification: Test transfer, teardown, and window isolation.**
  Astra runtime gaps; Opus F14/F15. `src/cascadia/TerminalApp/Tab.cpp:639`, `:1600`.
  Trace all Pane::Closed raise sites and whether TabViewItem is ever cleared while
  Tab remains alive. Then exercise pending title callbacks during detach, close,
  and movement between differently named windows. Null dispatcher and debug
  assertion crashes are hypotheses, not demonstrated bugs. Capture the destination
  dispatcher if the event can legitimately arrive off-thread.

- [ ] **R09 / P2 Verification: Cover the Conpty metadata provider.**
  Both reviewers. `src/cascadia/TerminalConnection/ConptyConnection.cpp:675`.
  Cover shell identity/PID, case-insensitive env lookup, overrides, WT variables,
  failed launch, replacement, and inbound handoff. Verify that
  snapshots survive expected lifecycle transitions and never log environment
  contents. No provider regression tests currently establish this behavior.

- [ ] **R10 / P2 Verification: Prove incremental ABI regeneration in Visual Studio.**
  Astra build gap; Opus F18 and runtime suspicions.
  `src/cascadia/TerminalSettingsAppAdapterLib/TerminalSettingsAppAdapterLib.vcxproj:39`.
  Use normal VS Build after changes to each IDL producer. Check regenerated
  projections, consumer objects, launch, and package payload for duplicate WinMD
  or XBF files. Audit ModelLib's file-only TerminalConnection metadata reference
  as a potential analogous gap; do not assume it reproduces the adapter failure.
  No CLI build. Existing references and acyclic XML alone do not prove VS behavior.

- [x] **R11 / P2 Verification: Strengthen integration assertions.**
  Both reviewers' test gaps. `src/cascadia/LocalTests_TerminalApp/TabTests.cpp:702`;
  `src/cascadia/UnitTests_SettingsModel/TitleFormatTests.cpp`.
  Use distinct pane profiles for MRU tests and independent expected pane titles
  rather than deriving expectations from the resolver under test. Add tab/window
  empty-result fallback, invalid window separator, full settings warning, and
  post-migration checkbox re-enable coverage.

## Design And Complexity Decisions

- [x] **R12 / P3: Remove or justify unused dependency sets.**
  Astra 4; Opus F4. `src/cascadia/inc/TitleFormat.h:78`, `:123`, `:184`.
  Only tests consume Dependencies(); production allocates/populates the set
  without using it. Prefer removal unless dependency-aware invalidation is needed.
  Correction to Opus: equal cwd reports do NOT notify; SetWorkingDirectory compares
  values. Do not claim per-prompt notification overhead without measurement.

- [x] **R13 / P3: Decide whether eager template parsing is worth retaining.**
  Opus F5. `src/cascadia/TerminalApp/Tab.cpp:397`, `:525`.
  Settings reload prepopulates a cache also filled lazily. This is redundant work
  in one sense, but supports the spec's parse-on-settings-change intent. Measure
  or simplify deliberately. Do NOT remove the whole WalkTree: it also performs
  pane->UpdateSettings, which is required existing behavior.

- [x] **R14 / P3: Document boxed-string setter and equality contracts.**
  Opus F8. `src/cascadia/TerminalSettingsModel/IInheritable.h:232`.
  Non-string IPropertyValue inputs throw and getters allocate fresh boxes. An
  invalid-input exception is not itself a defect; silently accepting bad types
  may be worse. Test rejection without mutation and compare GetString(), not COM
  identity. Only cache boxes if measurements justify extra state.

- [x] **R15 / P3: Review workspace-name ownership.**
  Opus F9. `src/cascadia/TerminalApp/Tab.h:215`; `Tab.cpp:570`;
  `TerminalPage.cpp:5973` in the same directory.
  Tabs cache page-owned window names and receive fan-out updates. No stale-state
  bug was established. Consider a shared context only if it simplifies transfers
  and ownership; first cover rename and cross-window movement (R08).

- [x] **R16 / P3: Decide environment storage lifecycle.**
  Opus F10; prior user discussion. `src/cascadia/TerminalConnection/ConptyConnection.h:96`.
  Base and finalized environments are both retained. They differ intentionally.
  Assess memory cost and whether to consolidate after launch. Lazy capture based
  only on initial formats breaks later settings reloads unless another source
  remains available. No new disclosure route was identified; the original map
  already retains much of this data. No consolidation authorized by this review.

- [x] **R17 / P3: Minimize metadata lock duration if warranted.**
  Opus F16 and lock-contention suspicion. `ConptyConnection.cpp:675` in the same
  directory. Query process metadata into locals before publishing under the lock
  if this materially simplifies locking. Measure launch-time/UI contention and
  heavy-output title queries; no deadlock or performance regression demonstrated.
  Cache-line alignment alone is not an actionable correctness fix.

- [x] **R18 / Product Decision: Confirm migration and checkbox UX.**
  Opus F2. `src/cascadia/TerminalSettingsModel/GlobalAppSettings.cpp:166`.
  Persisted null opts the user into legacy selection even after re-enabling the
  old checkbox; workspace prefix stays absent. This matches the specified null
  semantics and deliberate persisted migration, not a P1 implementation bug.
  Decide on discoverability/UI, test the re-enable path, and do not reinterpret
  explicit null or remove persistence without changing the requirement.

- [x] **R19 / Product Decision: Use Terminal host elevation only.**
  Opus F1. `src/cascadia/TerminalConnection/ConptyConnection.cpp:675`.
  User decision supersedes the original launch-client contract. Elevation means
  Terminal host elevation on both surfaces, independent of the active connection.
  Child-process, WSL and remote privilege tracking are explicitly out of scope.

- [x] **R20 / Product Decision: Literal dollar before expansion.**
  Opus F13. `src/cascadia/inc/TitleFormat.h:32`.
  Greedy `$${` escaping makes `$` immediately followed by an expansion difficult
  to express directly. This follows the specified escape rule. Decide whether
  syntax needs extending; add explicit dollar-run tests before changing it.

- [x] **R21 / P3 Hygiene: Add missing final newline.**
  Opus F12. `src/cascadia/TerminalSettingsModel/TitleFormatSettings.h:55`.
  Confirm file bytes and add a newline if absent; no functional impact.

## Recorded But Not Accepted As Fixes

- **Opus F11, copyright headers:** neighbouring style is not evidence of a
  mandatory policy. Agent instructions prohibit adding copyright/license headers
  unless explicitly requested. No automatic header additions.
- **Opus F17, global vs per-window settings:** the spec explicitly chooses global
  formats and separators. A hypothetical future per-window implementation is
  not a current defect; revisit with that feature rather than changing scope now.
- **Opus F18, linking default variation:** the report's claim that repo memory
  says the StaticLibrary default varies is incorrect. Both inspected v170/v180
  toolsets default it to false. Keep the incremental-build audit (R10), not that
  unsupported claim.
- **Opus F19, initial cwd imprecision for WSL:** explicitly accepted by the user
  when choosing existing WorkingDirectory. Do not restore reported-only state.
- **Opus F20, redundant trim characters:** harmless redundant character set,
  not a bug or meaningful cleanup priority.
- **Opus MIDL suspicion:** no evidence that IPropertyValue is illegal; it replaced
  the known-invalid IReference<String>. Include compilation in R10, not a new
  unsupported MIDL defect claim.

## Coverage Of Reviewer Reports

Astra 1-4 map to R01, R02, R03, R12. Its runtime/test gaps map to R07-R11.
Opus F1-F10 map to R19, R18, R04, R12, R13, R05/R03, R06, R14, R15, R16.
Opus F11-F20 map to recorded dispositions, R21, R20, R08, R08, R17,
recorded dispositions, R10/disposition, recorded dispositions, recorded dispositions.
Its runtime suspicions and test gaps are included in R07-R11 and R17.
