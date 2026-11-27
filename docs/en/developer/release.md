---
title: Release
description: Full NSUM release runbook: version bump, build/deploy, clean packaging, GitHub Release, metadata, verification and commits.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/developer/release.md
---

# Release

The complete sequence from "decide to release" to "downloadable online". Commands are copy-paste; the order matters (the installer snapshots embedded files at configure time).

## 0. Prerequisites

| Item | Requirement |
|---|---|
| Tools | `pwsh`, `cmake`, `git`, `gh` (logged in), `7z` |
| Build env | Dynamic Qt (`msvc` preset), static Qt prefix (installer preset) |
| Repo state | `main` synced with `origin/main`; only intended changes |
| Submodules | Initialized (`git submodule update --init --recursive`) |
| Features | Merged and verified per [Testing](./testing.md) |

```powershell
$repo = '<repo root>'
$out  = '<artifact output>'
$meta = '<metadata repo root>'
$v    = Get-Content (Join-Path $repo 'version.json') -Raw | ConvertFrom-Json
$ver  = $v.external
$full = "$($v.external).$($v.internal)"
```

> The release chain currently uses the `msvc` (Debug) preset for historical consistency.

## 1. Bump

```powershell
pwsh tools/bump_version.ps1 -Part external-patch
pwsh tools/bump_version.ps1 -Part internal-patch
```

External only on an explicit release request.

## 2. Build and deploy

```powershell
cmake --preset msvc
cmake --build build --config Debug --target neo_deploy
```

## 3. Clean the deploy tree

Remove `stores/`, `crash-report/`, `config/`, `*.log` before packaging; otherwise local state leaks into the release (a 734 MB incident).

## 4. Installer and CLI distribution

```powershell
cmake -S . -B build_nsum_installer -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=H:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_PREFIX_PATH=H:/Qt-static-rt/6.11.1/msvc2022_64
cmake --build build_nsum_installer --config Release --target hci_cli_dist hci_gui
```

## 5. Package

```powershell
7z a -tzip "$out/NSUM_Runtime_CLI_$ver.zip" .\build_nsum_installer\nsum_cli_dist\* -r '-xr!stores' '-xr!crash-report' '-xr!config' '-x!*.log'
7z a -tzip "$out/NSUM_Runtime_$ver.zip" .\build\deploy\* -r '-xr!stores' '-xr!crash-report' '-xr!config' '-x!*.log'
Get-FileHash "$out/NSUM_Runtime_CLI_$ver.zip" -Algorithm SHA256
```

## 6. GitHub Release

```powershell
gh release create "v$ver" --title "NSUM Runtime $ver" --notes-file "$out/notes_$ver.md" `
  "$out/NSUM_Runtime_CLI_$ver.zip" "$out/NSUM_Runtime_$ver.zip"
```

## 7. Metadata

Update `metadata/NSUM.json` (`Latest`, prepend a `List` entry with `download_link`, `release_page`, `sha_256`, `size_bytes`, `bridge_versions`) and `metadata/nsum-bridge.json` for a bridge release; copy jars under 10 MB into `public/nsum-bridge/`.

## 8. Verify online

```powershell
Invoke-RestMethod 'https://metadata.hibernet.top/api/metadata?type=NSUM'
```

- `Latest` matches;
- Asset HEAD returns 200 with the expected size;
- metadata changes take 30–60 s via Vercel.

## 9. Commit

```powershell
git add -A
git commit -m "release: NSUM $ver"
git push
```

## Rollback

| Step | Rollback |
|---|---|
| Runtime package | Republish the previous tag/package; update metadata |
| bridge | Republish the previous jar |
| metadata | Revert the metadata commit |

## Related

- [Versioning](./versioning.md)
- [Testing](./testing.md)
