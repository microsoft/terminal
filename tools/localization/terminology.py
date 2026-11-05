# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

"""In-memory TBX byte ranges, shared Aho-Corasick matching, and lazy concept lookup."""

from __future__ import annotations

import shutil
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from collections.abc import Iterator
from pathlib import Path
from xml.parsers import expat

import ahocorasick
from filelock import FileLock

URL = "https://download.microsoft.com/download/b/2/d/b2db7a7c-8d33-47f3-b2c1-ee5e6445cf45/MicrosoftTermCollection.zip"
XML_LANG = "{http://www.w3.org/XML/1998/namespace}lang"
CACHE_NAME = "windows-terminal-translation-terminology"
CACHE_MAX_AGE = 7 * 24 * 60 * 60
ARCHIVE_NAME = "MicrosoftTermCollection.zip"


def download_archive(path: Path) -> None:
    temporary = path.with_suffix(".download")
    try:
        with urllib.request.urlopen(URL, timeout=120) as response, temporary.open("wb") as target:
            while data := response.read(1024 * 1024):
                target.write(data)
        with zipfile.ZipFile(temporary) as archive:
            if not any(name.endswith(".tbx") for name in archive.namelist()):
                raise ValueError("Downloaded archive contains no TBX files")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def copy_cached_archive(destination: Path) -> None:
    cache = Path(tempfile.gettempdir()) / CACHE_NAME
    # Keep the lock outside the directory whose ctime determines freshness.
    with FileLock(str(cache) + ".lock", timeout=300):
        if cache.is_symlink() or cache.is_junction():
            raise ValueError(f"Terminology cache must not be a link: {cache}")
        try:
            created = cache.stat().st_birthtime
        except FileNotFoundError:
            created = None
        cached_archive = cache / ARCHIVE_NAME
        if created is not None:
            if not cache.is_dir() or any(path.name != ARCHIVE_NAME for path in cache.iterdir()):
                raise ValueError(f"Unexpected contents in terminology cache: {cache}")
            if cached_archive.is_symlink() or (cached_archive.exists() and not cached_archive.is_file()):
                raise ValueError(f"Invalid cached terminology archive: {cached_archive}")
        if created is None or time.time() - created > CACHE_MAX_AGE or not cached_archive.is_file():
            print("Refreshing Microsoft terminology cache...", flush=True)
            with tempfile.TemporaryDirectory(prefix=CACHE_NAME + "-", dir=cache.parent) as directory:
                staging = Path(directory)
                download_archive(staging / ARCHIVE_NAME)
                if created is not None:
                    cached_archive.unlink(missing_ok=True)
                    cache.rmdir()
                staging.rename(cache)
        # A run owns its snapshot even if another run refreshes the shared cache later.
        shutil.copyfile(cached_archive, destination)


def languages(locale: str) -> list[str]:
    parts = locale.lower().split("-")
    candidates = ["-".join(parts[:end]) for end in range(len(parts), 0, -1)]
    if locale.lower() in ("zh-cn", "zh-tw"):
        candidates.insert(1, "zh-hans" if locale.lower() == "zh-cn" else "zh-hant")
    return candidates


def archive_catalog(archive: zipfile.ZipFile) -> dict[str, str]:
    catalog = {}
    for name in archive.namelist():
        if not name.endswith(".tbx"):
            continue
        with archive.open(name) as stream:
            for _, element in ET.iterparse(stream, events=("start",)):
                if element.tag == "langSet" and element.get(XML_LANG) != "en-US":
                    lang = element.get(XML_LANG)
                    if not lang:
                        raise ValueError(f"Missing language in {name}")
                    if lang.lower() in catalog:
                        raise ValueError(f"Ambiguous terminology archive language: {lang}")
                    catalog[lang.lower()] = name
                    break
    return catalog


class Matcher:
    def __init__(self) -> None:
        self._automaton = ahocorasick.Automaton()
        self._ready = False

    def add(self, term: str) -> str:
        if self._ready:
            raise ValueError("Cannot add terms to a finalized matcher")
        key = term.casefold()
        if not key:
            raise ValueError("Empty terminology term")
        existing = self._automaton.get(key, None)
        if existing is not None:
            return existing
        self._automaton.add_word(key, key)
        return key

    def finalize(self) -> None:
        self._automaton.make_automaton()
        self._ready = True

    def matches(self, text: str) -> set[str]:
        if not self._ready:
            raise ValueError("Finalize the matcher before searching")
        if not len(self._automaton):
            return set()
        text = text.casefold()
        matches = set()
        for last, key in self._automaton.iter(text):
            start, end = last + 1 - len(key), last + 1
            if start and (text[start - 1].isalnum() or text[start - 1] == "_"):
                continue
            if end < len(text) and (text[end].isalnum() or text[end] == "_"):
                continue
            matches.add(key)
        return matches


class Collection:
    def __init__(self, raw: bytes, member: str, language: str, matcher: Matcher):
        self.member = member
        self._raw = raw
        self._matcher = matcher
        self._ranges: dict[str, list[tuple[int, int]]] = {}
        self._pairs: dict[str, set[tuple[str, str]]] = {}
        parser = expat.ParserCreate()
        parser.buffer_text = True
        entry_start: int | None = None
        english = False
        collecting = False
        terms: set[str] = set()
        text: list[str] = []

        def start(name: str, attributes: dict[str, str]) -> None:
            nonlocal entry_start, english, collecting
            if collecting:
                raise ValueError(f"Nested markup in English terminology term: {member}")
            if name == "termEntry":
                if entry_start is not None:
                    raise ValueError(f"Nested terminology concept: {member}")
                entry_start = parser.CurrentByteIndex
                terms.clear()
            elif name == "langSet":
                locale = attributes.get("xml:lang")
                if locale is None or locale.casefold() not in ("en-us", language):
                    raise ValueError(f"Unexpected terminology language {locale!r}: {member}")
                english = locale == "en-US"
            elif name == "term" and english:
                collecting = True
                text.clear()
                parser.CharacterDataHandler = text.append

        def end(name: str) -> None:
            nonlocal entry_start, english, collecting
            if name == "term" and collecting:
                parser.CharacterDataHandler = None
                collecting = False
                term = "".join(text)
                if term:
                    terms.add(matcher.add(term))
            elif name == "langSet":
                english = False
            elif name == "termEntry":
                if entry_start is None:
                    raise ValueError(f"Unexpected terminology concept end: {member}")
                # Expat reports byte offsets into the original UTF-8 buffer.
                offset = parser.CurrentByteIndex
                if terms:
                    finish = raw.find(b">", offset) + 1
                    if finish == 0:
                        raise ValueError(f"Unterminated terminology concept: {member}")
                    span = (entry_start, finish)
                    for term in terms:
                        self._ranges.setdefault(term, []).append(span)
                entry_start = None

        def reject_doctype(*args) -> None:
            raise ValueError(f"DTDs are not allowed in terminology: {member}")

        def declaration(version: str, encoding: str | None, standalone: int) -> None:
            if encoding and encoding.lower().replace("-", "") != "utf8":
                raise ValueError(f"Expected UTF-8 terminology: {member}")

        parser.StartElementHandler = start
        parser.EndElementHandler = end
        parser.StartDoctypeDeclHandler = reject_doctype
        parser.XmlDeclHandler = declaration
        parser.Parse(raw, True)
        if not self._ranges:
            raise ValueError(f"No English terminology terms found: {member}")

    def _concepts(self, key: str) -> Iterator[tuple[ET.Element, ET.Element, list[str], list[str]]]:
        for start, end in self._ranges.get(key, ()):
            # Only requested concepts become trees; descriptions otherwise remain raw bytes.
            entry = ET.fromstring(self._raw[start:end])
            english = entry.find(f"langSet[@{XML_LANG}='en-US']")
            if english is None:
                raise ValueError(f"Missing English terminology concept in {self.member}")
            sources = [term.text for term in english.iter("term")
                       if term.text and term.text.casefold() == key]
            targets = [term.text for lang in entry.findall("langSet") if lang.get(XML_LANG) != "en-US"
                       for term in lang.iter("term") if term.text]
            yield entry, english, sources, targets

    def pairs(self, keys: set[str]) -> set[tuple[str, str]]:
        pairs = set()
        for key in keys & self._ranges.keys():
            if key not in self._pairs:
                self._pairs[key] = {
                    (source, target) for _, _, sources, targets in self._concepts(key)
                    for source in sources for target in targets
                }
            pairs.update(self._pairs[key])
        return pairs

    def lookup(self, term: str) -> str:
        rows = {
            (entry.get("id", ""), source, target, english.findtext(".//descrip[@type='definition']", default=""))
            for entry, english, sources, targets in self._concepts(term.casefold())
            for source in sources for target in targets
        }
        return "\n".join(f"[{concept}] {source}: {target}\n  {definition}"
                         for concept, source, target, definition in sorted(rows)) \
            or f"No terminology matches for {term!r}."


def hints(text: str, collections: dict[str, Collection | None]) -> str:
    matcher = next((collection._matcher for collection in collections.values() if collection), None)
    if matcher is None:
        return ""
    keys = matcher.matches(text)
    matches: dict[str, dict[str, set[str]]] = {}
    for locale, collection in sorted(collections.items()):
        if collection:
            for source, target in collection.pairs(keys):
                matches.setdefault(source, {}).setdefault(locale, set()).add(target)
    return "\n".join(
        source + "\n" + "\n".join(f"  {locale}: {' | '.join(sorted(targets))}"
                                  for locale, targets in sorted(locales.items()))
        for source, locales in sorted(matches.items())
    )


def load_collections(archive: zipfile.ZipFile, locales: list[str]) -> dict[str, Collection | None]:
    catalog = archive_catalog(archive)
    matcher = Matcher()
    members: dict[str, Collection] = {}
    collections: dict[str, Collection | None] = {}
    for locale in locales:
        language = next((lang for lang in languages(locale) if lang in catalog), None)
        if language is None:
            collections[locale] = None
            continue
        member = catalog[language]
        if member not in members:
            members[member] = Collection(archive.read(member), member, language, matcher)
        collections[locale] = members[member]
    matcher.finalize()
    return collections
