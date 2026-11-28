---
title: Pointer Extensions
description: Editor pointer extensions (NeoEditorExtension_Pointer_*) for configuring resolvers and metadata.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/editor/pointer-extensions.md
---

# Pointer Extensions

Pointer editor extensions provide forms and validation for the editor's pointer panel.

## Built-in extensions

| Extension | resolver | Location |
|---|---|---|
| NeoEditorExtension_Pointer_DirectURL | `direct_url` | `editor/extension/pointers/` |
| NeoEditorExtension_Pointer_Modrinth | `modrinth` | same |

## Interface

Implements `NeoCore::IPointerEditorExtension`:

| Member | Purpose |
|---|---|
| `resolver_name()` | Matching runtime resolver |
| `fields()` | Form fields (key/label/required/placeholder) |
| `validate(metadata)` | Metadata validation |
| `build_metadata(values)` | Build pointer metadata JSON |

> Exact signatures: `modules/NeoCore/include/IPointerEditorExtension.h`.

## Usage

1. Open the pointer editor (toolbar/context menu);
2. Choose a resolver (DirectURL/Modrinth);
3. Fill fields (URL or project/version);
4. Save: writes `branch_config/<sha>.pointer` and updates `file_manifest`/`pointer_files`.

## Validation

- URL must be `http(s)://`;
- SHA-256 must be 64 hex chars;
- Modrinth `version_id`/`project_id` must not be empty;
- Saving is blocked when validation fails.

## Related

- [Editor extensions](./index.md)
- [Pointer resolvers](../pointers/index.md)
