---
title: ABI
description: Cross-DLL binary interface: exported symbols, calling convention, STL/Qt boundaries, logging, struct layout and versioning.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/framework/abi.md
---

# ABI

Plugins and the host run in the same process and interact across a DLL boundary. **Everything crossing that boundary must be ABI-stable.**

## Exported symbols

```cpp
extern "C" __declspec(dllexport) NeoCore::IConfigParser* CreateParser();
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();
extern "C" __declspec(dllexport) NeoCore::IModpackExporter* CreateExporter();
extern "C" __declspec(dllexport) NeoCore::IConfigEditorExtension* CreateConfigEditor();
extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension();
```

- `extern "C"` avoids name-mangling differences;
- **`__declspec(dllexport)` is mandatory**, otherwise the DLL has no exports and `GetProcAddress` fails (`CreateExporter not found`);
- Factories return interface pointers owned by the host.

## Calling convention

- Default `__cdecl` (unified on x64); never mix calling conventions on exported functions;
- Pass only C types/interface pointers/`const char*`; passing `std::string`/`std::vector`/`QString` requires the exact same compiler/STL/runtime on both sides and is high risk.

## C++ types inside interfaces

Existing interfaces (`IConfigParser`/`IPluginPointer`/`IModpackExporter`) do use `std::string`/`std::vector`/`nlohmann::json`:

- Precondition: host and plugin are built with the **same MSVC toolchain and runtime**;
- Broken by: different VS versions, `/MD` vs `/MT`, different STL or nlohmann versions;
- Rule: ship plugins and host built together; third-party plugins must use the same toolchain.

## Logging across DLLs

- `CLogger` lives in `CommonLoggerCPP` (STATIC), so each DLL has its own static instance;
- After init the host injects a sink via `SetPluginLogSink`;
- Plugins export the registration via `NEO_DECLARE_PLUGIN_LOG_SINK("name")`;
- Without injection, `CLogger::Resolve()` falls back to `spdlog::default_logger()`.

## Struct layout

- Structs crossing DLLs (`ParserCapability`, `ConfigEntry`, `PointerInfo`, `ExportMetadata`, `BuildTarget`) must have identical layout;
- **After changing members/order in a shared header, rebuild every target**, including TUs that `new` the type;
- Layout mismatch symptoms: `HEAP CORRUPTION 0xC0000374` on exit, random `0xC0000005`.

## Version compatibility

| Change | Compatibility |
|---|---|
| Appending a virtual to an interface | Old plugin vtable mismatch → must upgrade together |
| Changing enum values | Semantically incompatible |
| Adding meta fields | Loader tolerates unknown fields; old hosts ignore new ones |
| Internal-only plugin changes | Compatible |

Release rule: Runtime/plugins/editor extensions ship in the same version; bridge and Runtime interact via the CLI contract (JSON/exit codes), not a shared ABI.

## Examples

```cpp
// Correct: exported factory returning an interface pointer
extern "C" __declspec(dllexport) NeoCore::IPluginPointer* CreatePointer();

// Wrong: missing dllexport (GetProcAddress fails)
extern "C" NeoCore::IPluginPointer* CreatePointer();

// Wrong: returning a raw pointer/reference to std::string across the boundary
```

## Related

- [Plugin contracts](./plugin-contracts.md)
- [Extension registry](./extension-registry.md)
