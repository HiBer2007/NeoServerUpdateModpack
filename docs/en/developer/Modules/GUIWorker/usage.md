---
title: GUIWorker usage
description: GUIWorker public APIs and typical usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/GUIWorker/usage.md
---

# GUIWorker usage

## Quick start

See the module headers and the corresponding Chinese usage page for the full walkthrough.

## Public API

| Symbol | Description |
|---|---|
| `WizardWindow` | QWidget | Build wizard |\n| `BuildPage` | QWidget | Build progress |\n| `ModpackContentIde` | QWidget | Content IDE |\n| `PointerEditorPanel` | QWidget | Pointer editor |\n| `EditorExtensionRegistry` | class | Extension registry |

## Notes

- Rebuild all dependent targets after changing shared headers;
- Prefer the module's public headers over internal sources;
- Log English messages only, and keep paths UTF-8.

## Related

- [README.md](./README.md)