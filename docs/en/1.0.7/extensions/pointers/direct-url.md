---
title: DirectURL Resolver
description: NeoPointer_DirectURL downloads from the URL in the pointer metadata.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/pointers/direct-url.md
---

# DirectURL Resolver

`NeoPointer_DirectURL` is the simplest resolver: download from `metadata.url`.

## Capability

| Item | Value |
|---|---|
| resolver | `direct_url` |
| metadata | `url` (required), `size` (optional) |
| schemes | `https://` (recommended), `http://` |

## Pointer

```json
{
  "resolver": "direct_url",
  "metadata": { "url": "https://cdn.example.com/mods/example-1.2.3.jar" },
  "sha256": "<64 hex>",
  "size": 123456
}
```

## Create

Editor: right-click → Convert to pointer (computes SHA-256, moves to cache, picks DirectURL).

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/example.jar --sha256 <64-hex> `
    --resolver direct_url --url https://cdn.example.com/example.jar --branch base --repo <repository>
```

## Download behavior

- Parallel Range downloads when supported and large enough;
- Automatic single-stream fallback;
- Verifies sha256 and size after download;
- Deletes mismatched files and reports an error.

## Limitations

- URLs must stay stable;
- No credentials (auth URLs unsupported);
- No local-file scheme (put local files in the repo or `custom_mods`).

## Related

- [Pointer resolvers](./index.md)
- [Pointers (author)](../../pack-author/pointers.md)
