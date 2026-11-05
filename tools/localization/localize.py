# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

"""Cross-platform entry point: prepare -> Copilot SDK -> validate and merge."""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import subprocess
import sys
import tempfile
import zipfile
from contextlib import redirect_stdout
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from resw import Entry, Resource, fail, protection
import terminology


def git_root(directory: Path) -> Path:
    output = subprocess.run(
        ["git", "-C", str(directory), "rev-parse", "--show-toplevel"],
        check=True, stdout=subprocess.PIPE,
    ).stdout
    return Path(output.decode("utf-8").rstrip("\r\n")).resolve()


ROOT = git_root(Path(__file__).resolve().parent)
INSTRUCTIONS = Path(__file__).with_name("Translation-Instructions.md")
LOG = logging.getLogger(__name__)


def git(*args: str) -> bytes:
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          check=True, stdout=subprocess.PIPE).stdout


def read_optional(path: Path) -> bytes | None:
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return None


def atomic_write(path: Path, raw: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(dir=path.parent)
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def repository_path(relative: str) -> Path:
    path = (ROOT / relative).resolve()
    if not path.is_relative_to(ROOT) or relative.startswith(("/", "\\")):
        fail(f"Path outside the repository: {relative}")
    return path


def tracked_files(*folders: str, revision: str | None = None) -> set[str]:
    command = ("ls-tree", "-r", "--name-only", "-z", revision) if revision else ("ls-files", "-z")
    return set(git(*command, "--", *folders).decode("utf-8").split("\0")) - {""}


@dataclass
class Job:
    source: str
    target: str
    language: str
    keys: set[str]


def entry_payload(entry: Entry, languages: list[str], previous: dict[str, Resource]) -> dict:
    protections = {locale: protection(entry, locale) for locale in languages}
    return {
        "source": entry.value, "comment": entry.comment,
        "locked": protections[languages[0]]["locked"],
        "copy": [locale for locale, rules in protections.items() if rules["copy"]],
        "previous": {locale: resource.entries[entry.name].value for locale, resource in previous.items()
                     if entry.name in resource.entries},
    }


def batch_prompt(batch: dict) -> str:
    return (f"Languages: {', '.join(batch['languages'])}\n"
            f"Terminology candidates:\n{batch['terminology']}\n\n"
            f"Entries:\n{json.dumps(batch['entries'], ensure_ascii=False, separators=(',', ':'))}")


def make_batches(entries: list[dict], collections: dict[str, terminology.Collection | None], budget: int) -> list[dict]:
    import tiktoken

    encoder = tiktoken.get_encoding("o200k_base")

    def payload(items: list[dict]) -> dict:
        languages = sorted({locale for item in items for locale in item["languages"]})
        shared = [
            {**{key: value for key, value in item.items() if key != "languages"},
             "copy": [locale for locale in item["copy"] if locale in languages],
             "previous": {locale: value for locale, value in item["previous"].items() if locale in languages}}
            for item in items
        ]
        # Leave space for language expansion and a bounded structured tool response.
        output_tokens = 3 * len(encoder.encode(json.dumps(
            [{"id": item["id"], "values": dict.fromkeys(languages, item["source"])} for item in items],
            ensure_ascii=False, separators=(",", ":"),
        )))
        return {"languages": languages, "entries": shared,
                "entry_tokens": len(encoder.encode(json.dumps(shared, ensure_ascii=False, separators=(",", ":")))),
                "output_tokens": output_tokens}

    groups: list[list[dict]] = []
    for entry in entries:
        family = (entry["file"], entry["resource_id"].split(".", 1)[0])
        if not groups or (groups[-1][-1]["file"], groups[-1][-1]["resource_id"].split(".", 1)[0]) != family:
            groups.append([])
        groups[-1].append(entry)
    batches = []
    current: list[dict] = []
    for group in groups:
        candidate = payload(current + group)
        # Reserve half the input budget for glossary and context.
        if current and (candidate["entry_tokens"] > budget // 2 or candidate["output_tokens"] > 12000):
            batches.append(payload(current))
            current = []
            candidate = payload(group)
        if candidate["entry_tokens"] > budget // 2 or candidate["output_tokens"] > 12000:
            fail(f"Resource family exceeds batch budget: {group[0]['file']}:{group[0]['resource_id']}")
        current += group
    if current:
        batches.append(payload(current))
    for batch in batches:
        text = "\n".join(item["source"] for item in batch["entries"])
        batch["terminology"] = terminology.hints(text, {locale: collections[locale] for locale in batch["languages"]})
        batch["input_tokens"] = len(encoder.encode(batch_prompt(batch)))
    if any(batch["input_tokens"] > budget for batch in batches):
        fail("Terminology exceeds reserved input space; prepare smaller batches with --batch-tokens")
    return batches


def run(args) -> None:
    from agents import translate

    baseline = git("rev-parse", "--verify", "--end-of-options", f"{args.baseline or 'HEAD'}^{{commit}}").decode().strip()
    head = git("rev-parse", "HEAD")
    git("merge-base", "--is-ancestor", baseline, head.decode().strip())
    before = {path for path in tracked_files("src", revision=baseline) if path.endswith(".resw")}
    now = {path for path in tracked_files("src") if path.endswith(".resw")}
    if args.language:
        unsupported = set(args.language) - {PurePosixPath(path).parent.name for path in before | now}
        if unsupported:
            fail(f"Unknown target languages: {sorted(unsupported)}")

    resources: dict[str, Resource | None] = {}

    def read_resource(file: str) -> Resource | None:
        if file not in resources:
            raw = read_optional(repository_path(file))
            resources[file] = Resource(raw) if raw is not None else None
        return resources[file]

    jobs = []
    selected: dict[str, dict[str, set[str]]] = {}
    for source in sorted(path for path in before | now if PurePosixPath(path).parent.name == "en-US"):
        path = PurePosixPath(source)
        current = read_resource(source) if source in now else None
        resources[source] = current
        if source in now and current is None:
            fail(f"Missing tracked English source: {source}")
        previous = current if args.baseline is None else (
            Resource(git("show", f"{baseline}:{source}")) if source in before else None
        )
        locales = {
            PurePosixPath(candidate).parent.name for candidate in before | now
            if PurePosixPath(candidate).parent.parent == path.parent.parent
            and PurePosixPath(candidate).name == path.name
        } - {"en-US"}
        if source not in before and not locales:
            locales = {PurePosixPath(candidate).parent.name for candidate in now
                       if PurePosixPath(candidate).parent.parent == path.parent.parent} - {"en-US"}
        if args.language:
            locales &= set(args.language)
        for locale in sorted(locales):
            if locale.startswith("qps-"):
                continue
            target = str(path.parent.parent / locale / path.name)
            old = read_resource(target)
            keys = set()
            if current:
                for key, entry in current.entries.items():
                    base_entry = previous.entries.get(key) if previous else None
                    if not old or key not in old.entries or base_entry is None or (
                        entry.value, entry.comment
                    ) != (base_entry.value, base_entry.comment):
                        keys.add(key)
                        selected.setdefault(source, {}).setdefault(key, set()).add(locale)
                if not keys and old and current.render({
                    key: old.entries[key].value for key in current.entries
                }) == old.raw:
                    continue
            elif old is None:
                continue
            jobs.append(Job(source, target, locale, keys))

    needed = sorted({job.language for job in jobs if job.keys})
    collections: dict[str, terminology.Collection | None] = {}
    if needed:
        with tempfile.TemporaryDirectory(prefix="terminal-localization-") as directory:
            archive_path = Path(directory) / terminology.ARCHIVE_NAME
            terminology.copy_cached_archive(archive_path)
            with zipfile.ZipFile(archive_path) as archive:
                collections = terminology.load_collections(archive, needed)
    for locale, collection in collections.items():
        if collection is None:
            LOG.warning("No Microsoft terminology collection for %s", locale)
    entries = []
    ids: dict[tuple[str, str], str] = {}
    for file, keys in sorted(selected.items()):
        current = resources[file]
        assert current is not None
        path = PurePosixPath(file)
        localized = {}
        for locale in needed:
            old = read_resource(str(path.parent.parent / locale / path.name))
            if old:
                localized[locale] = old
        for key, languages in sorted(keys.items()):
            identifier = str(len(entries) + 1)
            ids[file, key] = identifier
            entry = entry_payload(current.entries[key], needed, localized)
            entry.update(id=identifier, resource_id=key, file=file, languages=sorted(languages))
            entries.append(entry)
    batches = {f"batch-{index:03d}": batch
               for index, batch in enumerate(make_batches(entries, collections, args.batch_tokens), 1)}
    count = sum(len(job.keys) for job in jobs)
    generated = sum(len(batch["entries"]) * len(batch["languages"]) for batch in batches.values())
    print(f"{count} selected translations; {generated} requested translations; {len(batches)} batches; "
          f"{len(jobs)} output files; {len({job.language for job in jobs})} languages.", flush=True)
    if batches:
        print(f"Largest batch: {max(b['input_tokens'] for b in batches.values())} input tokens; "
              f"{max(b['output_tokens'] for b in batches.values())} estimated output tokens.", flush=True)

    translated = asyncio.run(translate(batches, collections, args.model, args.timeout))
    pending: list[tuple[Path, bytes | None]] = []
    for job in jobs:
        target = repository_path(job.target)
        current, old = resources[job.source], resources[job.target]
        if current is None:
            pending.append((target, None))
            continue
        values = {key: entry.value for key, entry in old.entries.items() if key in current.entries} if old else {}
        values.update({key: translated[ids[job.source, key]][job.language] for key in job.keys})
        output = current.render(values)
        if not old or output != old.raw:
            pending.append((target, output))

    # Check the snapshot after constructing every output, before modifying any resource.
    if git("rev-parse", "HEAD") != head:
        fail("HEAD changed during localization")
    if {path for path in tracked_files("src") if path.endswith(".resw")} != now:
        fail("Tracked resource files were added or removed during localization")
    for file, resource in resources.items():
        if read_optional(repository_path(file)) != (resource.raw if resource else None):
            fail(f"Resource changed during localization: {file}")
    for target, output in pending:
        if output is None:
            target.unlink(missing_ok=True)
        else:
            atomic_write(target, output)
    print(f"Merged {len(pending)} resource files; full English key coverage and order verified.", flush=True)


class CommitDiagnostics(logging.StreamHandler):
    def __init__(self) -> None:
        super().__init__(sys.stderr)
        self.setLevel(logging.WARNING)
        self.setFormatter(logging.Formatter("%(levelname)s: %(message)s"))
        self.messages: dict[str, None] = {}

    def emit(self, record: logging.LogRecord) -> None:
        message = " ".join(record.getMessage().splitlines())
        self.messages[f"{record.levelname.title()}: {message}"] = None
        super().emit(record)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", help="Ancestor Git tag or commit; omit to use HEAD and fill only missing translations")
    parser.add_argument("--language", action="append", help="Limit to an existing locale; default all real locales")
    parser.add_argument("--batch-tokens", type=int, default=48000, help="Input estimate ceiling; output capped separately")
    parser.add_argument("--model", help="Optional Copilot model; default runtime selection")
    parser.add_argument("--timeout", type=int, default=7200, help="Whole orchestration timeout in seconds")
    args = parser.parse_args()
    diagnostics = CommitDiagnostics()
    logging.getLogger().addHandler(diagnostics)
    status = 0
    try:
        # The only stdout output is the final commit message, including on failure.
        with redirect_stdout(sys.stderr):
            if not 1000 <= args.batch_tokens <= 100000:
                fail("--batch-tokens must be between 1000 and 100000")
            if args.timeout <= 0:
                fail("--timeout must be positive")
            run(args)
    except KeyboardInterrupt:
        status = 130
        LOG.error("Localization interrupted")
    except Exception as error:
        # CLI boundary: fail explicitly, retain the traceback on stderr, and report it in stdout.
        status = 1
        LOG.exception("%s: %s", type(error).__name__, error)
    finally:
        logging.getLogger().removeHandler(diagnostics)
        title = "Update Windows Terminal translations" if status == 0 else "Windows Terminal localization failed"
        print(title + ("\n\n" + "\n".join(diagnostics.messages) if diagnostics.messages else ""))
    return status


if __name__ == "__main__":
    sys.exit(main())
