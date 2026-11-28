---
title: HiBerGUILibrary
description: Generic Qt widget library: tree panels, Git panel, progress cards, toast, file editor and deep tree behavior.
lang: en
status: complete
updated: 2026-11-27
translation_of: /zh/1.0.6/extensions/libraries/guibergui.md
---

# HiBerGUILibrary

`HiBerGUILibrary` (namespace `HiBerGUI`) is a business-agnostic Qt widget library reused by the NSUM editor and main program.

## Components

| Component | Purpose |
|---|---|
| RepoTreePanel / OutputTreePanel | Repository/output trees (with DeepTreeBehavior) |
| GitPanel | Async Git status/stage/commit/history |
| AnimatedProgress | Animated progress (compact mode) |
| ProgressCard | Loading/import progress card |
| ToastNotification | Lightweight toasts |
| FileContentEditor | Text/config editing |
| DeepTreeBehavior | Tree expand/collapse behavior (arrow/double-click) |

## DeepTreeBehavior

- Viewport event filter, no QTimer;
- Arrow click → expand one level / deep collapse;
- Double-click directories handled by the behavior (panels must not also connect `itemDoubleClicked`);
- Animation comes from the tree's `setAnimated(true)`.

## Usage

```cpp
#include <repo_tree_panel.h>
#include <deep_tree_behavior.h>

auto* tree = new HiBerGUI::RepoTreePanel(parent);
new HiBerGUI::DeepTreeBehavior(tree, tree);
```

## Notes

- GitPanel requires the host to inject the Git path: `setGitPath(GetDefaultGitPath())`;
- Components must not depend on NSUM business types;
- After changing shared header members, rebuild every dependent target (layout mismatch crashes).

## Related

- [Libraries](./index.md)
- [HiBerGUILibrary module](../../developer/Modules/HiBerGUILibrary/README.md)
