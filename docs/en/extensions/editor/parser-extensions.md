---
title: Parser Extensions
description: Editor config parser extensions (NeoEditorExtension_Parser_*): highlighting, formatting and key lists.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/editor/parser-extensions.md
---

# Parser Extensions

Config editor extensions let authors edit repository configs in the GUI and provide key lists for sync policies.

## Built-in extensions

| Extension | Extensions | Location |
|---|---|---|
| NeoEditorExtension_Parser_JSON | `.json` `.json5` `.jsonc` | `editor/extension/parsers/` |
| NeoEditorExtension_Parser_YAML | `.yml` `.yaml` | same |
| NeoEditorExtension_Parser_TOML | `.toml` | same |
| NeoEditorExtension_Parser_SNBT | `.snbt` | same |
| NeoEditorExtension_Parser_TXT | `.txt` `.properties` `.cfg` | same |
| NeoEditorExtension_Parser_Properties | `.properties` | same |

## Interface

Implements `NeoCore::IConfigEditorExtension`:

| Method | Purpose |
|---|---|
| `capability()` | Name/extensions |
| `parse(text)` | Parse into an editable structure |
| `serialize()` | Write back text |
| `list_keys()` | Key list for sync policies |

> Exact signatures: `modules/NeoCore/include/IConfigEditorExtension.h`.

## Behavior

- Double-clicking a config routes to the matching editor;
- Saving writes back to the repository and stages the Git change;
- Key lists feed `tracked_keys` in policy editing;
- Parse failures show an error and keep the original text.

## Adding one

1. Implement the interface and export the factory;
2. Provide `*.meta.json`;
3. Place it in `editor/extension/parsers/`;
4. Ship the matching runtime `NeoParser_*` if merging is needed.

## Related

- [Editor extensions](./index.md)
- [Config parsers](../parsers/index.md)
