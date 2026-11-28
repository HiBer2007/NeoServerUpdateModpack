---
title: Testing
description: NSUM's test layers and copy-paste commands: structural checks, E2E fixtures, bridge self-tests, real launch, packaging and docs validation.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/testing.md
---

# Testing

This page lists the verification order after code changes, pass criteria per layer, and how to prepare external test workspaces.

## Layers

| Layer | Goal | Entry | Pass criteria |
|---|---|---|---|
| Build | Compiles and deploys | `cmake --build build --target neo_deploy` | Exit 0; `build/deploy` has EXE, plugin DLLs, `docs/` |
| Structural | Version/plugins/repository valid | `info version`, `info plugins`, `info exporters`, `exec verify-repo` | Matches `version.json`; no `DLL missing` |
| Smoke | No exit crash/heap corruption after layout changes | Start GUI → wait 3–4 s → close | Exit 0, no new `crash-report`, no `HEAP CORRUPTION` |
| E2E fixture | Export/pointers/L1/L2/L3/misc | `run_e2e_cli.ps1` | `E2E-RESULT passed=20 failed=0` |
| bridge self-test | Import/sync/injection/failure rollback | `-Dnsum.selftest=true` etc. | `[NSUM-TEST]`/`[NSUM-E2E]` all PASS |
| Real launch | Game reaches the main menu after sync | `launch_real.ps1` + clean fixture | Sync `exit=0`; `Setting user` / `Sound engine started` |
| Packaging | Release zip clean and installable | 7-Zip listing + `hci_cli` silent install | No `stores/`, `crash-report/`, `config/`, `*.log`; install exit 0 |
| Docs | Bilingual parity, no dead links | `tools/docs_check.ps1` | `docs_check OK` (0 errors) |

> Functional acceptance is done by the maintainer; automated tests only prove "not detectably broken".

## Prerequisites

- `build/deploy` produced by the build commands in [Contributing](./contributing.md).
- The E2E fixture/tools live **outside the repository** (commonly `nsum_e2e`): `<E2E root>/repo`, `target`, `work`, `out`; scripts have a `$root` constant.
- bridge self-tests need the FLFHMCL workspace/testbed with the `nsum-bridge` jar in `<workspace>/testbed/mods/`.
- Non-ASCII paths are a key regression area; test Chinese/full-width paths too.

## Structural checks (unit-test equivalent)

There is no standalone C++ unit-test project in this repository (only the `nbtcpp` submodule has `enable_testing()`). The automatable "unit tests" are CLI contract/structure checks:

```powershell
$exe = '<repo>\build\deploy\NeoServerUpdateModpack.exe'
& $exe -v
& $exe info version --json
& $exe info plugins --json
& $exe info exporters --json
& $exe exec verify-repo --repo <repository> --json
```

## E2E

```powershell
pwsh -NoProfile -File <E2E tools>\run_e2e_cli.ps1
```

Covers export, pointers, L1/L2/L3 sync and misc; expects `passed=20 failed=0`.

## Docs

```powershell
pwsh -NoProfile -File tools/docs_check.ps1
```

## Notes

- Failure means stop and fix before continuing;
- Keep fixtures outside the repository;
- Always test with a non-ASCII path at least once per release.

## Related

- [Contributing](./contributing.md)
- [Release](./release.md)
