---
title: Versioning
description: NSUM dual version numbers (external/internal/full), version.json, bump script and injection points.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/versioning.md
---

# Versioning

NSUM uses two version numbers; `version.json` is the only handwritten source.

## Model

| Name | Form | Meaning | When to bump |
|---|---|---|---|
| External `external` | `x.x.x` | Public release version; Git tag (`v` + version) and Release name | Only when the maintainer announces a release |
| Internal `internal` | `x.x.x` | Internal build sequence for iterations between releases | After each task/feature batch |
| Full `full` | `x.x.x.x.x.x` | `external` + `internal`, used in resources/logs | Automatically |
| Git commit | short hash (`-dirty` possible) | Source state | Injected at build time |

Rationale: daily changes should not pollute the public version, but builds within one release must still be distinguishable.

## version.json

```json
{
  "external": "1.0.5",
  "internal": "0.1.11"
}
```

- `CMakeLists.txt` reads and validates both (`x.x.x`, otherwise `FATAL_ERROR`);
- Do not hand-edit versions elsewhere; `NSUM_INSTALLER_VERSION` is installer-only;
- `pack_format` (`.nsumpack` structure version, currently 2) is **not** a product version.

## bump_version.ps1

```powershell
pwsh tools/bump_version.ps1 -Part external-patch
pwsh tools/bump_version.ps1 -Part internal-patch
pwsh tools/bump_version.ps1 -Set external=1.2.3
pwsh tools/bump_version.ps1 -Show
```

| Part | Effect |
|---|---|
| `external-major/minor/patch` | `1.4.2` → `2.0.0` / `1.5.0` / `1.4.3` |
| `internal-major/minor/patch` | Same rules for the internal version |

## Injection

CMake defines globally:

- `NSUM_EXTERNAL_VERSION`
- `NSUM_INTERNAL_VERSION`
- `NSUM_FULL_VERSION`
- `NSUM_GIT_COMMIT`

Used by the Windows VERSIONINFO, startup banner, `info version` JSON and GUI status bar/about dialog.

## Notes

- Reconfigure (and clean if needed) after changing version macros;
- Release commits bump the external version only when requested;
- Keep `pack_format` bumps separate from product versions.

## Related

- [Release](./release.md)
- [ADR 0006](adr/0006-nsumpack-v2.md)
