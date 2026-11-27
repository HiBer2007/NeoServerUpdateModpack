---
title: NeoPointer_Modrinth usage
description: NeoPointer_Modrinth resolver name, metadata and API.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/Modules/NeoPointer_Modrinth/usage.md
---

# NeoPointer_Modrinth usage

## Quick start

```cpp
NeoBuild::PointerDownloader downloader;
auto result = downloader.resolve(pointerPath, repoDir);
```

## Capability

| Item | Value |
|---|---|
| resolver | `modrinth` |
| metadata fields | `project_id` (required), `version_id` (recommended) |

## Public API

| API | Signature | Description |
|---|---|---|
| `name` | `std::string name() const` | Resolver name |
| `can_handle` | `bool can_handle(const PointerInfo&) const` | Support check |
| `resolve_url` | `std::string resolve_url(const PointerInfo&)` | Resolve URL |
| `validate` | `bool validate(...)` | Post-download check |

## Notes

- URLs/versions must stay available;
- Failures must be explicit;
- Ship together with the matching editor extension.

## Related

- [README.md](./README.md)