# Diagram sources / 图源

Mermaid sources (`.mmd`) and exported SVG (`.svg`) for the inline diagrams in the documentation.
文档中的 Mermaid 图源与导出的 SVG（中英页面共用）。

| Source | SVG | Page / 页面 |
|---|---|---|
| `01-developer-architecture-build-pipeline.mmd` | `.svg` | developer/architecture/build-pipeline |
| `02-developer-architecture-data-model.mmd` | `.svg` | developer/architecture/data-model |
| `03-developer-architecture-data-model.mmd` | `.svg` | developer/architecture/data-model |
| `04-developer-architecture-downloader.mmd` | `.svg` | developer/architecture/downloader |
| `05-developer-architecture-index.mmd` | `.svg` | developer/architecture/index |
| `06-developer-architecture-overview.mmd` | `.svg` | developer/architecture/overview |
| `07-developer-architecture-overview.mmd` | `.svg` | developer/architecture/overview |
| `08-developer-architecture-pointer-system.mmd` | `.svg` | developer/architecture/pointer-system |
| `09-developer-architecture-security.mmd` | `.svg` | developer/architecture/security |
| `10-developer-architecture-sync-engine.mmd` | `.svg` | developer/architecture/sync-engine |
| `11-developer-framework-extension-registry.mmd` | `.svg` | developer/framework/extension-registry |
| `12-developer-framework-index.mmd` | `.svg` | developer/framework/index |
| `13-developer-framework-module-layout.mmd` | `.svg` | developer/framework/module-layout |
| `14-developer-Modules-CommonLoggerCPP-README.mmd` | `.svg` | developer/Modules/CommonLoggerCPP |
| `15-user-sync-and-launch.mmd` | `.svg` | user/sync-and-launch |

Regenerate:

```powershell
python tools/extract_diagrams.py
# render each .mmd to .svg (mermaid-cli or any Mermaid renderer)
```
