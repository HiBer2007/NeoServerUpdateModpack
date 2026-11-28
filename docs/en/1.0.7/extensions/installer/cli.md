---
title: HIC CLI
description: hci_cli options, exit codes and JSON output for headless installs.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/extensions/installer/cli.md
---

# HIC CLI

`hci_cli.exe` is the headless install shell: it reads `product.json` and flow files and installs with defaults or explicit options.

## Options

| Option | Description |
|---|---|
| `--gui` / `--tui` / `--cli` | Shell mode (default: product `defaultMode`) |
| `-s`, `--silent` | Non-interactive (default answers) |
| `--path <dir>` | Install directory override |
| `--lang en\|zh` | Language |
| `--product <file.json>` | Product config (default `./product.json`) |
| `--flow install\|uninstall\|path` | Flow (or a flow file) |
| `--json` | JSON protocol blocks on stdout |
| `--verbose` | Debug logging |
| `--extensions <dir>` | Extension directory |
| `--download-threads <1-16>` | Parallel connections per download |
| `--external-download <dir>` | List downloads instead of fetching (see external mode) |
| `--help` / `--version` | Help/version |

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Failure |
| 2 | Usage error |
| 10 | External download mode: files must be prepared externally |

## Examples

```powershell
# Silent install
hci_cli.exe --silent --cli --path "C:\NSUM"

# Prepare a download list
hci_cli.exe --external-download "C:\nsum-dl" --silent --cli

# JSON protocol
hci_cli.exe --json --silent --cli
```

## JSON output

- Progress/results are emitted as JSON blocks (`--json`);
- External mode adds an `hci/downloads-required` block.

## Notes

- `--path` overrides the product default;
- Silent mode uses product defaults for missing answers;
- Failures return 1 with the reason on stderr/logs.

## Related

- [Flow scripts](./flow.md)
- [External download mode](./external-download.md)
