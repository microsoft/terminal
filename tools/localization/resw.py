# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

"""Read resource semantics with Expat; patch only value spans in the original bytes."""

from __future__ import annotations

import re
from collections import Counter
from dataclasses import dataclass
from typing import NoReturn
from xml.parsers import expat
from xml.sax.saxutils import escape


def fail(message: str) -> NoReturn:
    raise ValueError(message)


@dataclass(frozen=True)
class Entry:
    name: str
    value: str
    comment: str
    start: int
    end: int
    opening: bytes
    closing: bytes


class Resource:
    def __init__(self, raw: bytes):
        self.raw = raw
        self.entries: dict[str, Entry] = {}
        self.newline = "\r\n" if b"\r\n" in raw else "\n"

        parser = expat.ParserCreate()
        stack: list[str] = []
        current: dict = {}

        def tag_end(start: int) -> int:
            quote = None
            for index in range(start, len(raw)):
                char = raw[index]
                if quote:
                    if char == quote:
                        quote = None
                elif char in (34, 39):
                    quote = char
                elif char == 62:
                    return index + 1
            fail("Unterminated XML tag")

        def start(name: str, attributes: dict[str, str]) -> None:
            stack.append(name)
            if len(stack) == 1 and name != "root":
                fail("Expected a resw root")
            if stack == ["root", "data"]:
                key = attributes.get("name")
                if not key or key in self.entries:
                    fail(f"Missing or duplicate resource ID: {key}")
                if "type" in attributes or "mimetype" in attributes:
                    fail(f"Only string resources are supported: {key}")
                current.clear()
                current.update(name=key, value="", comment="", values=0, comments=0)
            elif stack == ["root", "data", "value"]:
                current["values"] += 1
                offset = parser.CurrentByteIndex
                end = tag_end(offset)
                opening = raw[offset:end]
                current.update(start=offset, opening=opening, opening_end=end,
                               empty=opening.endswith(b"/>"))
            elif stack == ["root", "data", "comment"]:
                current["comments"] += 1
            elif len(stack) > 3 and stack[:2] == ["root", "data"]:
                fail(f"Nested markup in string resource: {current['name']}")

        def text(value: str) -> None:
            if stack in (["root", "data", "value"], ["root", "data", "comment"]):
                current[stack[-1]] += value

        def end(name: str) -> None:
            if stack == ["root", "data", "value"]:
                if current["empty"]:
                    current["end"] = current["opening_end"]
                    current["opening"] = current["opening"][:-2] + b">"
                    current["closing"] = b"</value>"
                else:
                    current["end"] = tag_end(parser.CurrentByteIndex)
                    current["closing"] = raw[parser.CurrentByteIndex:current["end"]]
            elif stack == ["root", "data"]:
                if current["values"] != 1 or current["comments"] > 1:
                    fail(f"Expected one value and at most one comment: {current['name']}")
                self.entries[current["name"]] = Entry(**{
                    key: current[key] for key in Entry.__dataclass_fields__
                })
            stack.pop()

        parser.StartElementHandler = start
        parser.EndElementHandler = end
        parser.CharacterDataHandler = text
        parser.StartDoctypeDeclHandler = lambda *args: fail("DTDs are not allowed")
        parser.XmlDeclHandler = lambda version, encoding, standalone: (
            fail(f"Expected UTF-8 XML, got {encoding}")
            if encoding and encoding.lower().replace("-", "") != "utf8" else None
        )
        parser.Parse(raw, True)

    def render(self, values: dict[str, str]) -> bytes:
        if set(values) != set(self.entries):
            fail(f"Coverage mismatch: missing={set(self.entries) - set(values)}, "
                 f"unexpected={set(values) - set(self.entries)}")
        chunks = []
        cursor = 0
        for key, entry in self.entries.items():
            chunks.append(self.raw[cursor:entry.start])
            value = values[key]
            if value == entry.value:
                chunks.append(self.raw[entry.start:entry.end])
            else:
                encoded = escape(value, {"\r": "&#13;"}).replace("\n", self.newline).encode("utf-8")
                chunks.append(entry.opening + encoded + entry.closing)
            cursor = entry.end
        chunks.append(self.raw[cursor:])
        output = b"".join(chunks)
        check = Resource(output)
        if list(check.entries) != list(self.entries) or any(
            check.entries[key].value != value for key, value in values.items()
        ):
            fail("XML round-trip changed translated values or resource order")
        return output


LOCK = re.compile(r'\{Locked(?:=(?P<terms>(?:"(?:\\.|[^"])*"|[^"}])*))?\}')
FORMAT = re.compile(
    r"\{\{|\}\}|\{[^{}]*\}|[{}]|"
    r"%(?:\d+\$)?[-+#0 ']*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn%]"
)


def protection(entry: Entry, language: str) -> dict:
    locked = []
    copy = bool(re.fullmatch(r"https?://\S+", entry.value))
    for directive in LOCK.finditer(entry.comment):
        terms = directive.group("terms")
        if not terms:
            copy = True
            continue
        for term in re.split(r',(?=(?:[^"]*"[^"]*")*[^"]*$)', terms):
            term = term.strip()
            if term.startswith('"') and term.endswith('"'):
                term = term[1:-1].replace('\\"', '"')
            if not term:
                fail(f"Empty locked term: {entry.name}")
            if term.casefold() == language.casefold():
                copy = True
            if term in entry.value and term not in locked:
                locked.append(term)
    return {"locked": locked, "copy": copy}


def validate_value(entry: dict, value: str) -> None:
    source = entry["source"]
    key = entry["id"]

    if source and not value.strip():
        fail(f"{key}: empty translation")

    if entry["copy"] and source != value:
        fail(f"{key}: whole-value lock changed")

    if Counter(FORMAT.findall(source)) != Counter(FORMAT.findall(value)):
        fail(f"{key}: format placeholders changed")

    for term in entry["locked"]:
        if source.count(term) != value.count(term):
            fail(f"{key}: locked term changed: {term}")

    def boundary_whitespace(s: str) -> tuple[str, str]:
        return s[:len(s) - len(s.lstrip())], s[len(s.rstrip()):]
    if boundary_whitespace(source) != boundary_whitespace(value):
        fail(f"{key}: boundary whitespace changed")

    if len(source.splitlines()) != len(value.splitlines()):
        fail(f"{key}: line break count changed")

    if re.search(r"[\x00-\x08\x0B\x0C\x0E-\x1F\uD800-\uDFFF\uFFFE\uFFFF]", value):
        fail(f"{key}: invalid XML character")


def validate_response(entries: list[dict], languages: list[str], response: object) -> dict[str, dict[str, str]]:
    if not isinstance(response, list):
        fail("Response must be a JSON array")
    expected = {entry["id"]: entry for entry in entries}
    values = {}
    for item in response:
        if not isinstance(item, dict) or set(item) != {"id", "values"} or (
            not isinstance(item["id"], str) or not isinstance(item["values"], dict)
        ):
            fail("Response objects must contain exactly a string id and a values object")
        key = item["id"]
        if key not in expected or key in values:
            fail(f"Unexpected or duplicate translation ID: {key}")
        translations = item["values"]
        if set(translations) != set(languages):
            fail(f"{key}: translation languages differ: missing={set(languages) - set(translations)}, "
                 f"unexpected={set(translations) - set(languages)}")
        entry = expected[key]
        for language, value in translations.items():
            if not isinstance(value, str):
                fail(f"{key}/{language}: translation must be a string")
            validate_value({**entry, "id": f"{key}/{language}", "copy": language in entry["copy"]}, value)
        values[key] = translations
    if set(values) != set(expected):
        fail(f"Missing translations: {sorted(set(expected) - set(values))}")
    return values
