---
title: Installer
description: HiBerCommonInstaller (HIC): CLI, flow scripts, extensions and external download mode.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/extensions/installer/index.md
---

# Installer (HIC)

NSUM's GUI installer and `hci_cli` are driven by **HiBerCommonInstaller**: install flows are JSON, steps are implemented by the core/extensions, and GUI/TUI/CLI shells reuse them.

## Components

| Component | Purpose |
|---|---|
| `hci_core` | Zero-Qt core: variables, flow execution, downloads, extraction, registry |
| `hci_flow` | JSON flows + Lua conditions (`when`) |
| `hci_ext` | Extension host (git/winget/apt…) |
| `hci_gui` / `hci_tui` / `hci_cli` | Shells |

## Pages

- [CLI](./cli.md): `hci_cli` options and exit codes;
- [Flow scripts](./flow.md): `install.json` steps, variables, conditions;
- [Extensions](./extensions.md): built-in extensions and loading;
- [External download mode](./external-download.md): `--external-download` and exit 10.

## NSUM install flow

```mermaid
flowchart LR
    A[Welcome] --> B[License]
    B --> C[Choose directory]
    C --> D[Optional Editor]
    D --> E[Download/extract deploy]
    E --> F[Bundled Git plan]
    F --> G[Write install.conf]
    G --> H[Shortcuts/Done]
```

## Related

- [Editor docs release](../../pack-author/editor-gui.md)
