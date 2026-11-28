---
title: NeoParser_SNBT usage
description: Capabilities and usage of NeoParser_SNBT.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/developer/Modules/NeoParser_SNBT/usage.md
---

# NeoParser_SNBT usage

## Quick start

```cpp
NeoCore::PluginLoader loader;
loader.ScanDirectory("parsers");
auto* parser = loader.FindParser("config/example");
auto keys = parser->list_keys("config/example");
auto merged = parser->merge_entries("config/example", {"a.b"}, remote, local);
```

## Capability

| Item | Value |
|---|---|
| name | `SNBT` |
| extensions | `.snbt` |
| supported_modes | full_sync / config_merge / no_sync |
| line_tracking | false |
| priority | 70 |

## Public API

| API | Signature | Description |
|---|---|---|
| `capability` | `ParserCapability capability() const` | Capability |
| `can_handle` | `bool can_handle(const std::string&) const` | Extension check |
| `parse_entries` | `std::vector<ConfigEntry> parse_entries(const std::string&)` | Parse |
| `merge_entries` | `std::string merge_entries(const std::string&, const std::vector<std::string>&, const std::string&, const std::string&)` | Merge |
| `list_keys` | `std::vector<std::string> list_keys(const std::string&)` | Key list |

## Notes

- Merge writes normalize formatting (comments may be lost);
- Invalid content fails and the caller falls back to full-file;
- Rebuild host and plugin together after interface changes.

## Related

- [README.md](./README.md)