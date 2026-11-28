---
title: Framework / Buses / ABI
description: NeoCore base framework, module layout, HIC event bus and service registry, plugin contracts and cross-DLL ABI.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.5/developer/framework/index.md
---

# Framework / Buses / ABI

This group explains how extensions plug in:

1. [Base framework](./base-framework.md): NeoCore interfaces, paths, logging, cancellation, errors;
2. [Module layout](./module-layout.md): target types, dependency direction, artifacts;
3. [Event bus](./event-bus.md): HIC `EventBus`/`ServiceRegistry` APIs and threading;
4. [Extension registry](./extension-registry.md): discovery, loading, registration, lifecycle;
5. [Plugin contracts](./plugin-contracts.md): parser/pointer/exporter/editor-extension contracts;
6. [ABI](./abi.md): exported symbols, cross-DLL boundaries, layout and versioning.

## Relationship

```mermaid
flowchart LR
    Host[Host EXE] --> Core[NeoCore]
    Host --> Loader[PluginLoader / ExtensionLoader]
    Loader --> P1[Parser DLL]
    Loader --> P2[Pointer DLL]
    Loader --> P3[Exporter DLL]
    Loader --> P4[Editor extension DLL]
    Host --> Bus[HIC EventBus]
    Host --> Svc[ServiceRegistry]
```

## Principles

- Plugins depend only on NeoCore interface headers;
- Plugins export factories via `extern "C"`;
- The host injects logging (`SetPluginLogSink`); plugins never initialize logging;
- Do not pass STL containers/Qt objects across DLL boundaries unless the ABI explicitly allows it.

## Related

- [Module reference](../Modules/_meta.json)
- [Docs style guide](../../../../_meta/style-guide.md)
