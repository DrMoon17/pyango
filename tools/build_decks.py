#!/usr/bin/env python3
"""
build_decks.py - Converts the decks in decks/<edition>/*.tsv into firmware headers.

Usage (from the repository root):
    python3 tools/build_decks.py

It generates one header per edition:
    decks/en/*.tsv  ->  firmware/pyango/decks_en.h   (explanations in English)
    decks/es/*.tsv  ->  firmware/pyango/decks_es.h   (explicaciones en español)

Deck format (TSV = columns separated by TAB):
    # name: Japanese             <- name shown in the main menu
    # order: 1                   <- position in the main menu
    # script: cjk | latin        <- cjk = kanji/kana font, latin = Latin alphabet
    category<TAB>word<TAB>reading<TAB>pronunciation<TAB>meaning
    numbers<TAB>一<TAB>いち<TAB>ichi<TAB>one

Valid categories: numbers, verbs, positions, everyday
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DECKS_DIR = ROOT / "decks"
OUT_DIR = ROOT / "firmware" / "pyango"
COLUMNS = ["category", "word", "reading", "pronunciation", "meaning"]
CATEGORIES = ["numbers", "verbs", "positions", "everyday"]
EDITIONS = ["en", "es"]


def c_str(text: str) -> str:
    """Escape text for a C string literal."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def read_deck(path: pathlib.Path) -> dict:
    meta, cards = {}, []
    header_seen = False
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        if line.startswith("#"):
            m = re.match(r"#\s*(\w+)\s*:\s*(.*)", line)
            if m:
                meta[m.group(1).lower()] = m.group(2).strip()
            continue
        fields = [f.strip() for f in line.split("\t")]
        if not header_seen:
            if [f.lower() for f in fields] != COLUMNS:
                sys.exit(f"{path}:{n}: header must be: {' <TAB> '.join(COLUMNS)}")
            header_seen = True
            continue
        if len(fields) != len(COLUMNS):
            sys.exit(f"{path}:{n}: expected {len(COLUMNS)} columns, found {len(fields)}")
        cat, word, reading, pron, meaning = fields
        if cat not in CATEGORIES:
            sys.exit(f"{path}:{n}: unknown category '{cat}' (use: {', '.join(CATEGORIES)})")
        if not word or not meaning:
            sys.exit(f"{path}:{n}: 'word' and 'meaning' are required")
        cards.append((CATEGORIES.index(cat), word, reading, pron, meaning))

    if not cards:
        sys.exit(f"{path}: the deck is empty")
    script = meta.get("script", "latin").lower()
    if script not in ("latin", "cjk"):
        sys.exit(f"{path}: 'script' must be latin or cjk")
    # Cards are grouped by category, keeping the file order inside each one
    cards.sort(key=lambda c: c[0])
    return {
        "id": re.sub(r"[^a-zA-Z0-9]", "_", path.stem).lower(),
        "name": meta.get("name", path.stem),
        "order": int(meta.get("order", 99)),
        "script": script,
        "cards": cards,
    }


def build_edition(edition: str) -> None:
    paths = sorted((DECKS_DIR / edition).glob("*.tsv"))
    if not paths:
        sys.exit(f"No decks in decks/{edition}/")
    decks = sorted((read_deck(p) for p in paths), key=lambda d: d["order"])

    out = [
        f"// GENERATED FILE by tools/build_decks.py - do not edit by hand.",
        f"// Edit the decks in decks/{edition}/*.tsv and run the script again.",
        "#pragma once",
        '#include "deck_types.h"',
        "",
    ]
    for d in decks:
        out.append(f"// {d['name']} ({len(d['cards'])} cards)")
        out.append(f"static const Card cards_{edition}_{d['id']}[] = {{")
        for cat, *texts in d["cards"]:
            out.append(f"  {{{cat}, " + ", ".join(c_str(t) for t in texts) + "},")
        out.append("};")
        out.append("")

    out.append("static const Deck DECKS[] = {")
    for d in decks:
        script = "SCRIPT_CJK" if d["script"] == "cjk" else "SCRIPT_LATIN"
        out.append(f"  {{{c_str(d['name'])}, {script}, cards_{edition}_{d['id']}, {len(d['cards'])}}},")
    out.append("};")
    out.append(f"static const uint8_t NUM_DECKS = {len(decks)};")
    out.append(f"static const uint16_t MAX_CARDS = {max(len(d['cards']) for d in decks)};")
    out.append("")

    target = OUT_DIR / f"decks_{edition}.h"
    target.write_text("\n".join(out), encoding="utf-8")
    print(f"[{edition}] {target.relative_to(ROOT)}")
    for d in decks:
        print(f"      {d['name']:<10} {len(d['cards']):>4} cards ({d['script']})")


def main() -> None:
    for edition in EDITIONS:
        build_edition(edition)


if __name__ == "__main__":
    main()
