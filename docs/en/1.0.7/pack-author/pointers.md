---
title: Pointers
description: Creating, converting, resolving and troubleshooting pointer files.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/pack-author/pointers.md
---

# Pointers

Pointers keep large files (jars) out of Git: the repository stores a SHA-256 placeholder and clients download on demand and verify.

## Location

```text
branch_config/<sha256>.pointer
branch_config/<branch>.json
  ├─ file_manifest: "mods/a.jar" → "<sha256>"
  └─ pointer_files: "<sha256>" → { resolver, metadata }
```

> The pointer itself has no target path; the path is linked by `file_manifest`.

## Create

Editor: right-click a file → "Convert to pointer", choose resolver (DirectURL/Modrinth).

CLI:

```powershell
NeoWorkspaceEditor.exe exec pointer-add --path mods/a.jar --sha256 <64-hex> `
    --resolver direct_url --url https://example.com/a.jar --branch base --repo <repository>
```

## Resolution

1. Build detects a missing/mismatched `mods/a.jar`;
2. Look up the sha in `file_manifest`;
3. Check `<cache>/<sha>`; on hit verify and copy;
4. On miss: resolve the URL with the resolver → download → verify → cache.

## Cache and verification

- Build side: `.minecraft/versions/.cache/<sha256>`;
- Client embedded files: `.hmcl/nsum/cache/files/<sha256>`;
- Recompute SHA-256 before every cache use (zero trust).

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `No resolver found` | Resolver missing/misspelled | Check `pointers/` and `resolver` |
| `SHA-256 hash mismatch` | Corrupted cache/URL content changed | Delete cache and retry; update the pointer |
| 404 | Direct link dead | Change URL or use Modrinth |
| File disappeared after conversion | Conversion moves it into the cache | Normal; verify `file_manifest` |

## Notes

- Hashes must match content; regenerate after changes;
- `pointer_files` and `.pointer` files must stay in sync (editor/CLI write both);
- Do not hand-edit the sha field;
- For Modrinth pin `version_id` to avoid drift.

## Related

- [Sync rules](./sync-rules.md)
- Pointer resolvers (DirectURL/Modrinth): see the online extension docs
