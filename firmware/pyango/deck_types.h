// deck_types.h - Data structures for Pyango decks
#pragma once
#include <stdint.h>

enum Script : uint8_t {
  SCRIPT_LATIN = 0,  // Latin alphabet (Spanish, German, English...)
  SCRIPT_CJK   = 1   // Kanji and kana (Japanese)
};

// Categories (same order as CATEGORIES in tools/build_decks.py)
enum Category : uint8_t {
  CAT_NUMBERS = 0,
  CAT_VERBS,
  CAT_POSITIONS,
  CAT_EVERYDAY,
  CAT_COUNT
};

struct Card {
  uint8_t category;
  const char* word;      // what you study (shown big)
  const char* reading;   // kana (Japanese) or plural / past tense
  const char* pron;      // romaji or pronunciation guide
  const char* meaning;   // translation
};

struct Deck {
  const char* name;      // shown in the main menu
  Script script;
  const Card* cards;
  uint16_t count;
};
