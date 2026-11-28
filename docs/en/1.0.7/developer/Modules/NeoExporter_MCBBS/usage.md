---
title: NeoExporter_MCBBS usage
description: NeoExporter_MCBBS format, fields and usage.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/Modules/NeoExporter_MCBBS/usage.md
---

# NeoExporter_MCBBS usage

## Quick start

```cpp
auto* exporter = NeoBuild::ModpackExporter::create("exporters", "mcbbs");
NeoCore::BuildTarget target;
target.branch = "base";
target.git_branch = "main";
target.metadata.name = "My Pack";
target.metadata.version = "1.0.0";
auto r = exporter->build_modpack(target, &progress, &cancel);
exporter->export_modpack(buildDir, "out.zip", target.metadata);
```

## Format

| Item | Value |
|---|---|
| format | `mcbbs` |
| extension | `.zip` |
| fields | `name`, `version` (required); `author`, `description` |

## Public API

| API | Signature | Description |
|---|---|---|
| `format_name` | `std::string format_name() const` | Format |
| `file_extension` | `std::string file_extension() const` | Extension |
| `format_description` | `std::string format_description() const` | Description |
| `build_modpack` | `BuildResult build_modpack(const BuildTarget&, IBuildProgress*, CancelToken*)` | Build |
| `export_modpack` | `bool export_modpack(const std::string&, const std::string&, const ExportMetadata&)` | Export |

## Notes

- NSUM exporter writes a manifest by default; `embed_patterns` embeds files;
- HMCL exporter writes a directory, not a file;
- Field declarations drive GUI forms and CLI extra args.

## Related

- [README.md](./README.md)