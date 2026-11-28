---
title: GitIgnoreMarkup
description: Marked .gitignore blocks and an editing GUI for safe, tool-managed ignore rules.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/extensions/libraries/gitignore-markup.md
---

# GitIgnoreMarkup

GitIgnoreMarkup manages **block markers** in `.gitignore` so tools can add/remove their own rules without touching hand-written lines.

## Concepts

| Concept | Description |
|---|---|
| Marked block | A rule range wrapped in start/end comments |
| Marker name | Block identifier (e.g. `nsum-build`) |
| Reverse markup | Infer block ownership from an existing `.gitignore` |

## Usage

- Open ignore-rule editing from the editor's extensions/repository settings;
- Select a block → add/remove rules;
- Saving rewrites only the marked block; other lines are untouched.

## Rules

- Content outside marked blocks is never modified;
- Duplicate block names keep the last one and warn;
- Deduplication/sorting happens only inside blocks.

## Notes

- Removing marker comments turns the block into ordinary rules;
- Line endings (CRLF/LF) are normalized on write;
- Depends only on Qt Core plus the editor GUI components.

## Related

- [Libraries](./index.md)
- [GitIgnoreMarkup module](../../developer/Modules/GitIgnoreMarkup/README.md)
