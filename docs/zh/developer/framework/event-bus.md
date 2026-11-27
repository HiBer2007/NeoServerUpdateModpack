---
title: 事件总线
description: HIC 的 EventBus / ServiceRegistry API、线程语义，以及 NSUM 主程序的注册表机制。
lang: zh
status: complete
updated: 2026-11-27
---

# 事件总线

## 两个不同机制

| 机制 | 位置 | 用途 |
|---|---|---|
| HIC `EventBus` / `ServiceRegistry` | `modules/HiBerCommonInstaller/core/include/hci/bus.h` | 安装器内拓展与宿主通信 |
| NSUM 插件注册表 | `NeoCore::PluginLoader` / `ModpackExporter` / `PointerDownloader` | Runtime 按类型发现插件 |

> NSUM 主程序**没有**统一事件总线；不要假设存在 `EventBus` 全局单例。

## HIC EventBus

```cpp
hci::EventBus bus;
auto id = bus.subscribe("install.progress", [](const nlohmann::json& p) { /* ... */ });
bus.publish("install.progress", {{"percent", 42}});  // 同步
bus.post("install.progress", {{"percent", 43}});     // 入队
bus.drain();                                         // 派发队列
bus.unsubscribe(id);
bus.unsubscribeAll("install.progress");
```

语义：

- `publish`：同步调用该 topic 的全部 handler（注册顺序）；
- `post`：只入队，下一次 `drain()` 派发；
- `publishing_` 标志防止重入（回调内 publish 的处理见实现）；
- 线程约束：`publish`/`drain` 在调用线程执行；跨线程投递用 `post`。

## ServiceRegistry

```cpp
hci::ServiceRegistry services;
services.registerService<MyService>(&svc);
auto* s = services.service<MyService>();
```

- 按类型名（`typeid`）注册/查找；
- 用于拓展之间、宿主与拓展之间的能力查询；
- 生命周期由宿主管理，注销用 `unregisterService`。

## 安装器拓展接入

`IHciExtension` 在加载后注册：

- 步骤处理器（`registerStepHandler`）；
- 服务（`ServiceRegistry`）；
- 参数处理器。

`ExtensionLoader` 三种加载方式：

| 方式 | 说明 |
|---|---|
| `loadStatic()` | 链接期 `HCI_REGISTER_EXTENSION` 注册 |
| `loadDirectory(dir)` | `*.dll`（导出 `HciGetExtension`）+ `*.hci` 包 |
| `loadDll/loadPackage` | 单文件加载 |

## NSUM 插件发现

- `PluginLoader::ScanDirectory`：parser；
- `ModpackExporter`/`PointerDownloader`：扫描同名 DLL + meta；
- 编辑器：`editor/extension/*` 扫描注册。

## 失败模式

| 现象 | 原因 |
|---|---|
| 扩展步骤报 `unknown step type` | 拓展未加载/未注册 |
| 订阅无回调 | topic 拼写不一致 / 未 `drain` |
| 重入死循环 | publish 内再次 publish 同一 topic |

## 相关

- [扩展注册表](./extension-registry.md)
- [插件契约](./plugin-contracts.md)
