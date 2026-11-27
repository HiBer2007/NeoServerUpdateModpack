---
title: NeoWorkspaceEditor usage
description: NeoWorkspaceEditor public APIs and typical usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoWorkspaceEditor/usage.md
---

# NeoWorkspaceEditor usage

## Quick start

See the module headers and the corresponding Chinese usage page for the full walkthrough.

## Public API

| Symbol | Description |
|---|---|
| `EditorWindow` | QMainWindow | GUI |\n| `editor_cli::run` | function | CLI |\n| `promoteEditorDocs` | function | Release pack-author docs |

## Notes

- Rebuild all dependent targets after changing shared headers;
- Prefer the module's public headers over internal sources;
- Log English messages only, and keep paths UTF-8.

## Related

- [README.md](./README.md)