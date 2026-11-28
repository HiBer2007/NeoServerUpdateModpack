---
title: NeoEditorExtension_Parser_JSON usage
description: NeoEditorExtension_Parser_JSON interface and registration.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoEditorExtension_Parser_JSON/usage.md
---

# NeoEditorExtension_Parser_JSON usage

## Quick start

The editor scans `editor/extension/parsers/` at startup and registers the extension automatically.

## Public API

| API | Signature | Description |
|---|---|---|
| `createEditor` | `QWidget* createEditor(QWidget* parent)` | Create editor widget |
| capability/register | see `IConfigEditorExtension` header | Capability/registration |

## Notes

- The factory must be exported;
- Extensions and runtime plugins ship in the same version;
- Validation failures must not corrupt the original file;
- Rebuild all extensions after interface changes.

## Related

- [README.md](./README.md)