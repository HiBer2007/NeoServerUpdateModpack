---
title: NeoPointer_DirectURL usage
description: NeoPointer_DirectURL resolver name, metadata and API.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/Modules/NeoPointer_DirectURL/usage.md
---

# NeoPointer_DirectURL usage

## Quick start

```cpp
NeoBuild::PointerDownloader downloader;
auto result = downloader.resolve(pointerPath, repoDir);
```

## Capability

| Item | Value |
|---|---|
| resolver | `direct_url` |
| metadata fields | `url` (required), `size` (optional) |

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