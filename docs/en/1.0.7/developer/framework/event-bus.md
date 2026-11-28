---
title: Event Bus
description: HIC EventBus / ServiceRegistry APIs and threading, plus NSUM's own registry mechanisms.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.7/developer/framework/event-bus.md
---

# Event Bus

## Two different mechanisms

| Mechanism | Location | Purpose |
|---|---|---|
| HIC `EventBus` / `ServiceRegistry` | `modules/HiBerCommonInstaller/core/include/hci/bus.h` | Installer extensions ↔ host |
| NSUM plugin registries | `NeoCore::PluginLoader` / `ModpackExporter` / `PointerDownloader` | Runtime plugin discovery by type |

> The NSUM main program has **no** unified event bus; do not assume a global `EventBus`.

## HIC EventBus

```cpp
hci::EventBus bus;
auto id = bus.subscribe("install.progress", [](const nlohmann::json& p) { /* ... */ });
bus.publish("install.progress", {{"percent", 42}});  // synchronous
bus.post("install.progress", {{"percent", 43}});     // queued
bus.drain();                                         // dispatch queue
bus.unsubscribe(id);
bus.unsubscribeAll("install.progress");
```

Semantics:

- `publish`: synchronously invokes all handlers for the topic in registration order;
- `post`: queues only; delivered on the next `drain()`;
- `publishing_` guards reentrancy (see implementation);
- Threading: `publish`/`drain` run on the calling thread; cross-thread delivery uses `post`.

## ServiceRegistry

```cpp
hci::ServiceRegistry services;
services.registerService<MyService>(&svc);
auto* s = services.service<MyService>();
```

- Registered/looked up by type name (`typeid`);
- Used for extension-to-extension and host-to-extension capability queries;
- Lifetime is owned by the host; unregister with `unregisterService`.

## Installer extension wiring

`IHciExtension` registers after loading:

- Step handlers;
- Services (`ServiceRegistry`);
- Argument handlers.

`ExtensionLoader` load modes:

| Mode | Description |
|---|---|
| `loadStatic()` | Link-time `HCI_REGISTER_EXTENSION` |
| `loadDirectory(dir)` | `*.dll` (exports `HciGetExtension`) + `*.hci` packages |
| `loadDll/loadPackage` | Single-file loading |

## NSUM plugin discovery

- `PluginLoader::ScanDirectory`: parsers;
- `ModpackExporter`/`PointerDownloader`: scan matching DLL + meta;
- Editor: scans `editor/extension/*`.

## Failure modes

| Symptom | Cause |
|---|---|
| `unknown step type` | Extension not loaded/registered |
| No callback | Topic mismatch / `drain` not called |
| Reentrant loop | publish inside publish on the same topic |

## Related

- [Extension registry](./extension-registry.md)
- [Plugin contracts](./plugin-contracts.md)
