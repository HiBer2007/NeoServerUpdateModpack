---
title: NeoEditorExtension_Pointer_DirectURL usage
description: NeoEditorExtension_Pointer_DirectURL interface and registration.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoEditorExtension_Pointer_DirectURL/usage.md
---

# NeoEditorExtension_Pointer_DirectURL usage

## Quick start

The editor scans `editor/extension/pointers/` at startup and registers the extension automatically.

## Public API

| API | Signature | Description |
|---|---|---|
| `createEditor` | `QWidget* createEditor(QWidget* parent)` | Create editor widget |
| capability/register | see `IPointerEditorExtension` header | Capability/registration |

## Notes

- The factory must be exported;
- Extensions and runtime plugins ship in the same version;
- Validation failures must not corrupt the original file;
- Rebuild all extensions after interface changes.

## Related

- [README.md](./README.md)