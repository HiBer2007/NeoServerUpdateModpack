---
title: Pointer Resolvers
description: NSUM pointer resolver extensions that turn pointer metadata into download URLs.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/pointers/index.md
---

# Pointer Resolvers

Pointer files are named by the target SHA-256 (`branch_config/<sha256>.pointer`) and store only "how to fetch"; **resolvers** turn that into a URL.

## Overview

| Resolver | Capability | Typical use |
|---|---|---|
| NeoPointer_DirectURL | Direct download | Any HTTPS link, CI artifacts, mirrors |
| NeoPointer_Modrinth | Modrinth API | Official URLs via `project_id`/`version_id` |

## Contract

Implement `NeoCore::IPluginPointer`:

| Method | Purpose |
|---|---|
| `name()` | Resolver name |
| `can_handle()` | Metadata support |
| `resolve_url()` | Produce a URL |
| `validate()` | Post-download verification |

Factory:

```cpp
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();
```

## Pointer format

```json
{
  "resolver": "direct_url",
  "metadata": { "url": "https://example.com/mods/example.jar" },
  "sha256": "<64 hex>",
  "size": 123456
}
```

## Resolve and cache

```mermaid
flowchart LR
    A[file_manifest missing target] --> B[Read <sha>.pointer]
    B --> C{Cache hit?}
    C -- yes --> D[Recompute SHA-256]
    C -- no --> E[resolver.resolve]
    E --> F[Download]
    F --> D
    D -->|match| G[Write instance/cache]
    D -->|mismatch| H[Delete and error]
```

## Related

- [DirectURL](./direct-url.md) · [Modrinth](./modrinth.md)
- [Pointers (author)](../../pack-author/pointers.md)
