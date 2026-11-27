---
title: Installation
description: Install NSUM Runtime, nsum-bridge and the optional Editor, with verification steps.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/user/installation.md
---

# Installation

![Figure 1-2 NSUM installer welcome page](../../assets/screenshots/screen-installer.png)

On the client, NSUM consists of **HMCL**, **nsum-bridge** (an HMCL extension) and **NSUM Runtime** (the sync engine). The Editor is optional and only needed by pack authors.

## Option 1: automatic install inside HMCL (recommended)

1. Install the FLFHMCL runtime (Fabric injection layer); see the FLFHMCL docs.

<!-- slot:FLFHMCL_INSTALL -->

> **TBD**: a link will be added once the FLFHMCL installation page is ready.

2. Put `nsum-bridge-<version>.jar` into the `mods/` folder of your HMCL working directory.
3. Start HMCL; the **NSUM** page appears in the left sidebar.
4. On the NSUM page click "Install / Update Runtime". bridge downloads the latest Runtime package from the metadata site into:

```text
<HMCL working directory>/.hmcl/nsum/runtime/
```

5. The page shows the installed Runtime version; accept the prompt when an update is available.

## Option 2: standalone installer

If the server provides `NSUM_Installer_<version>.exe`:

1. Run the installer and choose the install directory;
2. Optionally tick "Editor" — **this is what releases the pack-author documentation**;
3. The installer downloads bundled Git (MinGit) and writes `install.conf`; the runtime does not depend on system Git;
4. The result contains `NeoServerUpdateModpack.exe` and a `deploy/` tree (or the install directory itself is the deploy root).

## Option 3: portable deploy

Extract `NSUM_Runtime_<version>.zip` anywhere; the folder containing `NeoServerUpdateModpack.exe` is the runtime root. Install/update bundled Git with:

```powershell
.\NeoServerUpdateModpack.exe exec git-update
```

## Verify

```powershell
# Version (full version looks like 1.0.5.0.1.11)
.\NeoServerUpdateModpack.exe info version

# Git source (bundled means bundled Git is usable)
.\NeoServerUpdateModpack.exe info git
```

In HMCL:

- the NSUM page shows the Runtime version and the install/update button;
- importing a `.nsumpack` recognizes the **NSUM modpack** type.

## Directories

| Path | Content |
|---|---|
| `.hmcl/nsum/runtime/` | Runtime deployment |
| `.hmcl/nsum/downloads/` | Runtime package download cache |
| `.hmcl/nsum/repos/` | Cloned/created NSUM repositories |
| `.hmcl/nsum/logs/` | bridge sync logs |
| `.hmcl/nsum/cache/files/` | SHA-256 cache for embedded pack files |

## Uninstall

- HMCL route: remove `mods/nsum-bridge-*.jar` and optionally `.hmcl/nsum/` (keep `repos/` if you want);
- Installer route: use "Apps & features" or the installer's uninstall flow.

## Common installation issues

| Symptom | Cause | Fix |
|---|---|---|
| NSUM page says Runtime is not installed | Only bridge was installed | Click "Install Runtime" on the NSUM page |
| Install stuck at 0 B/s | Network/proxy stalls large downloads | Retry, or download the zip manually into `.hmcl/nsum/downloads/` and use "Install from downloads" |
| SHA-256 mismatch | Incomplete or tampered download | Delete the file and retry |
| `git` unavailable | Bundled Git missing and no system Git | Run `exec git-update` or check `install.conf` |
