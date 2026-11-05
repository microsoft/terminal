# Incremental localization experiment

Python 3.12+ and [uv](https://docs.astral.sh/uv/) are required. The locked Python
Copilot SDK manages its own pinned native runtime: no npm, PowerShell, or separate
CLI installation is needed. The SDK downloads that runtime on first use; local
runs use your Copilot login, while CI uses `GITHUB_TOKEN` with
`copilot-requests: write` and the organization's corresponding Copilot policy.

## One command, locally or in CI

Python handles translation and read-only Git operations: baseline lookup,
tracked-file discovery, repository search, and HEAD validation. It does not stage
files, create commits/tags, merge branches, or push. By default, HEAD supplies the
repository baseline and all existing translations are trusted, even when English
resources have uncommitted changes. Only missing keys/files need translation.
From the repository root in PowerShell:

```powershell
uv run --locked --project tools\localization python tools\localization\localize.py > commit-message.txt
```

Supply `--baseline your-checkpoint-tag` to also translate English strings changed
since an ancestor tag or commit. An explicit `--baseline HEAD` compares against
the committed English resources, including uncommitted changes in that comparison.

On Linux, use `/` path separators and normal shell line continuation. Use
`--language de-DE` (repeatable) to limit the run; by default all existing real
locales are included, but not `en-US` or the three `qps-*` pseudo-locales.
`--model` optionally chooses a model; otherwise the SDK/runtime default applies.

Jobs, batches, and validated translations stay in memory for the single run.
There is no manifest, input/response-file protocol, work-directory, or resume
interface. A temporary terminology ZIP copy is deleted immediately after loading.
The checkout
contains the final resource changes for review and manual touchups.
Stdout contains a commit message: a title followed by warnings/errors, if any.
Progress and failure tracebacks go to stderr. On failure the message says
localization failed and the process exits nonzero; never commit that output.
Warnings include missing terminology, rejected tool calls, and linguistic concerns
reported by the translator. Identical diagnostics are deduplicated. A successful
run with no changes still prints a title; the caller checks the Git diff.
There is no HTML report, publication option, or artifact handoff. Inspect the
resource changes in your editor and use the message with `git commit -F` after review.

The flow is:

1. **Prepare once:** when a baseline is explicitly supplied, compare English keys,
   decoded values, and translator comments against that commit. Otherwise trust
   existing translations. Add every missing target key to the worklist, even if its
   English text is unchanged. Read each resource file's actual locale coverage,
   not the union of every locale used by other files in the same project.
2. **One SDK orchestrator:** delegate batches to fresh `translator` sub-agents,
   at most four concurrently. Each English key belongs to exactly one batch and
   agent, which researches its meaning once and translates it into all batch
   languages. Several projects may share a batch. Scoped Python tools load inputs,
   look up full Microsoft definitions, and validate/retain responses. Native
   `grep`, `glob`, and `view` tools provide application context.
   The LLMs get no shell, arbitrary file-write, or publication tools. A parent
   agent cannot submit translations itself.
3. **Merge once:** validate every response before touching resources, combine new
   translations with old values, then replace only `<value>` spans in the original
   English bytes. English order, schema comments, indentation, BOM, attributes,
   and line endings are preserved. Missing coverage, duplicates, unknown IDs,
   changed placeholders/locked terms, and invalid XML fail explicitly. Obsolete
   target keys disappear; deleted source files remove corresponding targets.
   The batch requests the union of languages needed by its keys. All keys are
   translated into that union, but only originally selected key/locale pairs are
   applied. Surplus translations never replace existing values or create locale
   coverage that the original plan did not request.

The English document is always the template. Even a file with no translations
needed may change to reflect English ordering, comments, or removal of old keys.
This intentionally differs from keeping stale target metadata. Missing files
or locale directories are recreated when supported at the checkpoint.
Resources are read and parsed once, and their original bytes are retained for
merging. After constructing all outputs and before writing any, the run checks
that HEAD, tracked resource paths, and the resource contents have not changed,
including previously missing files. Changes fail rather than overwrite newer
work. Instructions are read once when starting the agents; batch inputs and
validated submissions pass directly between functions without serialization to
disk or repeated validation. Resource discovery uses Git's tracked-file
list, including staged additions; untracked files are ignored for resource
planning. Research uses the runtime's native `grep`, `glob`, and `view`, retaining
their schemas, search behavior, and output handling. Any path inside the Git root
is readable, including untracked files and any extension; native search ignore
rules still apply. The root is discovered with `git rev-parse --show-toplevel`
from the script directory, not inferred from a fixed directory depth or the
caller's working directory.

The XML editor uses Expat byte offsets, not a document reserializer or regex XML
parser. For example, the entire introductory schema comment is copied unchanged.
Only translated values are escaped; UTF-8 input is required and DTDs/nested value
markup are rejected. All outputs are constructed and checked for exact key order
and 100% coverage before writing; individual file replacements are atomic.
An I/O failure still fails the run (there is no multi-file filesystem transaction).

The SDK manages its own session workspace. Its context manager only disconnects
and normally retains resumable state; we detach and call `delete_session` in a
`finally` block. The user's Copilot home, login, and shared runtime caches remain
untouched. No scratch-path grants are needed: batches and results remain in Python
memory and are accessed only through the scoped tools. A callback pre-tool hook checks
native read/search paths against the resolved Git root, including relative paths,
multiple search roots, and symlink/junction targets. The permission handler
approves only in-root reads; other permission requests, sandbox bypass, and
requests requiring managed approval are rejected. The pinned runtime's native
recursive searches do not follow directory links. Repository-origin file hooks,
skills, and configuration discovery remain disabled; callback hooks still run.
This is an application-level policy, not an OS sandbox for Python or protection
against concurrent filesystem changes. The Python host and runtime are trusted.

## Batches and terminology

The default input ceiling is 48,000 estimated tokens (`--batch-tokens`), with half
reserved for glossary/context. Batch boundaries use the entries alone, with the
complete terminology hints checked against the combined input ceiling.
An independent 12,000-token output estimate, allowing 3x English expansion,
includes the entire keys-by-languages matrix. Adding a key can expand the batch's
language union, so both estimates are recomputed for the complete candidate batch.
Consecutive properties of a resource UID in the same source file stay together.
There is no arbitrary 60/100-entry limit. An indivisible oversized family fails.
Token estimates use `tiktoken`'s `o200k_base`; they are a budgeting proxy, not the
exact tokenizer or a guarantee of any model's advertised context/output limit.
Repository research, instructions, and reasoning need additional headroom.

Keys are identified internally by English source file and resource ID, not text.
Preparation assigns deterministic, run-local sequential string IDs in sorted
source-file/resource-ID order. Each entry supplies its source text, comment,
file, resource ID, and shared locked terms once. Existing translations are
locale-indexed, and `copy` lists the locales with whole-value locks. Entries use
compact JSON; responses contain one `{"id":"1","values":{"de-DE":"...","fr-FR":"..."}}`
object per key with exactly every batch language. Validation checks each localized
value, complete matrix coverage, and unique key ownership across batches.

The official [Microsoft terminology ZIP](https://learn.microsoft.com/en-us/globalization/reference/microsoft-terminology)
is cached automatically in `windows-terminal-translation-terminology` under the
system temporary directory. It is fetched when the cache directory is missing,
its `ctime` is older than seven days, or the archive is missing. Refresh downloads
and validates the replacement first, then recreates the cache directory. A failed
download fails the run rather than using stale terminology. Concurrent runs share
a lock outside the cache directory; reads do not extend its lifetime.

Each run copies the cached ZIP to its own scratch directory and decompresses each
needed TBX collection once. Locales using the same collection share it in memory.
Expat indexes English terms and concept byte ranges without constructing an XML
tree or copying descriptions. A shared native `pyahocorasick` automaton searches
the English terms once per finalized batch, sharing the matches across all its
locales, retaining overlapping matches and
checking Unicode word boundaries. Matching and definition lookup use Unicode
casefolding; this also equates spellings such as `Straße` and `STRASSE`.

Only concepts needed for hints or explicit definition lookups are decoded into
small temporary XML trees. Matched translation pairs are reused; descriptions
remain in the original UTF-8 byte buffers, not duplicated in database rows.
There is no SQLite or persistent parsed-index cache. The temporary ZIP copy is
removed after loading; in-memory collections are released after the run and the
shared ZIP download remains.
Directory `ctime` is creation time on Windows
and metadata-change time on Unix; the cache directory is not modified on reuse.
Exact locale/script matches are preferred, then parent
languages (including `zh-Hans`/`zh-Hant`). Missing collections are reported
explicitly and have no hints. No definitions are pasted by default: all matching
term pairs are supplied as uncapped plain text, grouped by English term:

```text
pane
  de-DE: Bereich
  fr-FR: volet
```

Alternatives within a locale are separated by ` | `. Sub-agents can query all
matching concepts and definitions for an exact English term with `lookup_term`,
which takes their batch ID and returns results for every batch language.
Concepts and definitions remain distinct; equal English spellings do not imply
equal meanings.

The commit message intentionally does not enumerate changed strings: the Git diff
already does that. Linguistic quality and UI fit still need human review.

## CI

[localization-test.yml](../../.github/workflows/localization-test.yml) runs on pushes
to `dev/lhecker/llm-localization` or manual dispatch on that branch. It uses
Ubuntu 24.04 with explicitly installed, pinned uv and Python 3.13.

There is **one job and runner**, not a matrix. It creates a local-only fake
tag six weeks before the source commit's committer timestamp, passes that tag
to Python for all locales, and captures stdout as the commit message.
The full-coverage rule can select substantially more work than
the English diff alone, because existing missing translations are now repaired.
No fake tag is pushed or advanced.

The same Python invocation validates all responses and merges them into the
English template. Only after the SDK session has been shut down and Python exits
successfully does the workflow's shell code stage, commit, and push the results.
There is no second preparation pass, artifact upload/download, or artifact replay.
The shell requires a clean checkout and rejects changes outside localized resource
files. It runs `git add -A`, commits only if there are changes, and force-pushes
the current checkout to `dev/lhecker/llm-localization-test` without opening a PR.
Each run replaces the previous test result rather than merging its history.
An explicit `--force-with-lease` checks the test branch's commit captured before
translation (or requires the branch to remain absent on first creation), so a
concurrent update is rejected. Superseding source pushes are also rejected.
No worktrees or resource-copying step are needed. All Git writes belong to the
CI shell; the source branch is never pushed.

The job has `contents: write` and `copilot-requests: write` throughout, and checkout
retains Git credentials for the final push. This deliberately removes the former
separate-runner credential boundary. Agents still have only the explicitly scoped
SDK tools: no shell, out-of-root reads, arbitrary writes, or Git/publication tool. The Python
host and its dependencies are trusted, not sandboxed by those tool restrictions.

## Tests

Run the focused offline regression tests from the repository root:

```powershell
uv run --locked --project tools\localization python -m unittest discover -s tools\localization -p "test_*.py"
```

Tests cover multilingual batching and budgets, terminology, locale-specific locks,
agent ownership, sparse merging, Git-root discovery, and research access checks.
They use synthetic resources and terminology and a mocked SDK session: no
downloads or model calls. They also check incomplete sessions and concurrent
checkout changes cannot produce partial resource updates.
