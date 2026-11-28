---
title: Flow Scripts
description: HIC JSON flows: step types, variables, conditions and optional steps.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/installer/flow.md
---

# Flow Scripts

HIC flows are JSON: `product.json` defines the product/defaults; `install.json`/`uninstall.json`/`upgrade.json`/`repair.json` define step sequences.

## Structure

```json
{
  "id": "install",
  "steps": [
    { "type": "download", "url": "...", "dest": "${installDir}/pkg.zip" },
    { "type": "extract", "source": "${installDir}/pkg.zip", "dest": "${installDir}" }
  ]
}
```

## Step types

| Type | Purpose |
|---|---|
| `download` | Download (cpr → curl segments → PowerShell single stream fallback) |
| `extract` | Extract zip |
| `copy` | Copy files/directories |
| `clean` | Clean target (supports `skip`) |
| `run` | Run an external command |
| `script` | Inline script/commands |
| `template` | Write a templated file |
| `shortcut` | Create shortcuts (can be disabled) |
| `registry` | Write the registry |
| `git_plan` | Plan bundled Git (reuse/download) |
| `git_finalize` | Locate Git and write the path |
| `git_refresh` | Refresh Git environment |

> `git_*` come from the `ext/git` extension; other extensions register additional types; unknown types fail with `unknown step type: <x>`.

## Variables and conditions

```json
{
  "type": "download",
  "url": "https://example.com/editor.zip",
  "when": "vars.withEditor == 'true'",
  "optional": true
}
```

- `when`: Lua expression; skipped when false;
- `optional`: failures only warn;
- External download pause (exit 10) takes precedence over `optional`.

## Notes

- Steps execute in order; download failures abort by default;
- `clean` uses `skip` to protect `tools/` (never delete freshly installed Git);
- Update `install_cli.json` when changing flows.

## Related

- [CLI](./cli.md)
- [External download mode](./external-download.md)
