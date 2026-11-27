---
title: Installer Extensions
description: HIC extension host and built-ins (git/winget/apt, NSUM argument extension).
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/installer/extensions.md
---

# Installer Extensions

HIC extensions are pluggable step providers: features the core does not know (Git, package managers, product-specific args) are registered by extensions.

## Built-ins

| Extension | Provides | Steps/capabilities |
|---|---|---|
| `ext/git` (hci.git) | Bundled Git planning/install/locate | `git_plan`, `git_finalize`, `git_refresh` |
| `ext/winget` (hci.winget) | Windows package manager | `winget` steps |
| `ext/apt` (hci.apt) | Debian package manager | `apt` steps |
| `nsum_installer/ext/nsum_args_ext` | NSUM args | `--with-editor` etc. |

## Loading

- The installer uses `--extensions <dir>`;
- Extensions are DLLs registering step types/commands;
- Without the matching extension, a step type fails with `unknown step type`.

## Writing one

1. Implement the HIC extension interface (`hci/extension.h`);
2. Export the registration function;
3. Provide a `*.json` declaration;
4. Place it in the extension directory and reference its step type.

## NSUM argument extension

`nsum_args_ext` adds product-specific arguments:

| Argument | Purpose |
|---|---|
| `--with-editor` | Install the Editor and release pack-author docs |

## Notes

- Extension failures abort the flow unless the step is `optional`;
- The extension directory must be readable before the flow starts;
- Version mismatches may fail registration; check logs.

## Related

- [Flow scripts](./flow.md)
- [CLI](./cli.md)
