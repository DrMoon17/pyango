# Pyango decks

Each deck is a `.tsv` file (columns separated by **TAB**). You can edit it with any text editor, or with Excel / Google Sheets (export as "Tab-separated values").

```
decks/
├── en/   English edition: learn Japanese, Spanish and German (explanations in English)
└── es/   Edición en español: aprende inglés, alemán y japonés (explicaciones en español)
```

## Format

```
# name: German               ← name shown in the main menu
# order: 3                   ← position in the main menu
# script: latin              ← latin (Latin alphabet) or cjk (Japanese kanji/kana)
# license: CC BY-SA 4.0
category	word	reading	pronunciation	meaning
everyday	die Katze	die Katzen	KAT-seh	the cat
```

| Column | Required | Japanese | Other languages |
|---|---|---|---|
| `category` | yes | `numbers`, `verbs`, `positions` or `everyday` | same |
| `word` | yes | kanji (日本) | word, with article for nouns (die Katze) |
| `reading` | no | kana (にほん), max. 8 characters | plural or past tense |
| `pronunciation` | no | romaji (nihon) | pronunciation guide |
| `meaning` | yes | translation | translation |

The category menu is built automatically: it only shows the categories a deck actually has, plus **(All together)**.

## Adding or editing a deck

1. Create or edit a `.tsv` file in `decks/en/` or `decks/es/`.
2. From the repository root, run `python3 tools/build_decks.py`.
3. Upload the firmware to your ESP32 again.

## Contributing

Speak another language? Decks are the easiest way to contribute, no coding needed. Open a Pull Request with your `.tsv` file. All decks are published under **CC BY-SA 4.0**.
