# EditorDemo 使用文档

## 快速开始

### 构建

随主配置 `msvc` 预设一起构建：

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset msvc
& $cmake --build build --target editor_demo
```

产物：`build/modules/EditorDemo/editor_demo.exe`（该目录下另有 windeployqt 部署的 Qt DLL 与 WebView2Loader.dll；`NeoInstaller`/主程序构建不受影响）。

### 运行

```powershell
.\build\modules\EditorDemo\editor_demo.exe        # 默认停在「Qt 版编辑器」标签
.\build\modules\EditorDemo\editor_demo.exe --web  # 启动即切到「WebView2 版编辑器」标签
```

Web 标签页依赖系统 WebView2 运行时：缺失时编辑器区域显示错误占位，程序不崩溃（`WebCodeEditor` 内部 `activatePlaceholder` 回退）。

## 公共 API

本模块为演示 EXE，无对外库 API。公共面 = 命令行参数 + 演示窗口；真正可复用 API 全部来自 HiBerGUI（`code_editor_interface.h` 等，签名逐字摘自头文件）。

### 命令行

| 参数 | 说明 |
|------|------|
| `--web` | 启动后自动切换到 WebView2 版编辑器标签（索引 1），便于对比与诊断 |

### 演示窗口（main.cpp 内部，非导出 API）

`DemoWindow : QMainWindow`：三标签 `Qt 版编辑器` / `WebView2 版编辑器` / `merge 预览对比`。每个编辑器页提供：语言下拉（`builtinLanguages()`）、Tab 宽度（2/4）、载入 JSON / properties 样本、深色/浅色切换、只读/可编辑切换；merge 页提供两版 `MergePreviewDialog` 打开按钮。

### 用到的 HiBerGUI 组件 API（摘自头文件）

```cpp
// code_editor_interface.h
QStringList builtinLanguages();   // json/yaml/properties/toml/snbt/txt/plain

enum class CodeEditorKind { Qt, Web };              // Qt=纯 C++/Qt, Web=WebView2

using CodeEditorFactoryFn = ICodeEditor* (*)(QWidget* parent);
void registerCodeEditorFactory(CodeEditorKind kind, CodeEditorFactoryFn fn);
ICodeEditor* createCodeEditor(CodeEditorKind kind, QWidget* parent);

class ICodeEditor {                                  // 抽象接口（两版实现一致）
public:
    virtual QWidget* widget() = 0;
    virtual void setLanguage(const QString& langId) = 0;
    virtual QString language() const = 0;
    virtual void setPlainText(const QString& text) = 0;
    virtual QString toPlainText() const = 0;
    virtual void setReadOnly(bool ro) = 0;
    virtual bool isReadOnly() const = 0;
    virtual void setLineNumbers(bool on) = 0;
    virtual void setFontSize(int pt) = 0;
    virtual void setTabWidth(int spaces) = 0;        // 2/4，默认 4
    virtual void setDarkMode(bool dark) = 0;
    virtual void setRegionHighlights(const QVector<RegionHighlight>& regions) = 0;
    virtual void registerHighlighter(ICodeHighlighter* h) = 0;
    virtual void addAction(const EditorAction& action) = 0;
    virtual void scrollToTop() = 0;
};

struct RegionHighlight {                             // 行/列区间背景标记
    int startLine = 1;  int endLine = 1;             // 1-based 含端
    QString color;      QString tag;                 // 深色主题背景色 / 扩展标识
    int startColumn = -1; int endColumn = -1;        // 0-based；<0 = 整行
    QString colorLight;                              // 浅色主题背景色；空 = 跟随 color
};
struct EditorAction {                                // 工具条扩展动作
    QString id; QString text; QString tooltip;
    std::function<void(QWidget*)> handler;
};
```

```cpp
// web_code_editor.h（HiBerGUIWebEditor）
namespace HiBerGUI {
class WebCodeEditor : public QWidget, public ICodeEditor { /* 同 ICodeEditor 接口 */ 
public:
    bool usingWebView() const;                       // WebView2 是否已就绪
};
ICodeEditor* createWebCodeEditor(QWidget* parent);   // 工厂：由 Web 库调用注册
}
```

```cpp
// merge_preview_dialog.h（HiBerGUILibrary）
namespace HiBerGUI {
class MergePreviewDialog : public QDialog {
public:
    explicit MergePreviewDialog(CodeEditorKind kind, QWidget* parent = nullptr);
    void setContent(const QString& content, const QString& info,
        const QString& langId,
        const QVector<RegionHighlight>& highlights = {});
    ICodeEditor* editor() const;
};
}
```

## 典型用法

```cpp
// main.cpp 关键流程：注册 Web 工厂 → 组装窗口
registerCodeEditorFactory(CodeEditorKind::Web,
    [](QWidget* parent) -> ICodeEditor* {
        return createWebCodeEditor(parent);
    });

// Qt 版编辑器（CodeEditor 直接实例化）
auto* editor = new HiBerGUI::CodeEditor(page);
editor->setLanguage("json");
editor->setPlainText(sampleJson());
editor->setDarkMode(true);                // 默认即深色
editor->setTabWidth(4);
editor->setRegionHighlights({ { 3, 4, "#2f6b31", "tracked" } });   // 追踪键标记
editor->addAction({ "fmt", "缩进检查", "扩展动作示例",
    [](QWidget* w) { /* handler */ } });

// merge 预览（两版对比）
MergePreviewDialog dlg(CodeEditorKind::Qt, this);
if (dlg.editor()) dlg.editor()->setDarkMode(qtDark_);
dlg.setContent(sampleMerge(), "说明文本", "json",
    { { 2, 4, "#8a6d1a", "overwrite", -1, -1, "#f7e8a8" },   // 覆盖行（黄）
      { 5, 5, "#2f6b31", "tracked",   2, 33, "#b7e4c7" } }); // 追踪键值（绿）
dlg.exec();
```

样本数据（`main.cpp` 内置 `sampleJson` / `sampleProps` / `sampleMerge`）：JSON 为含 branches/sync_policies 的 workspace 示例，properties 为 `server.properties`（server-port/online-mode/max-players/motd/difficulty），merge 样本演示 `mode: partial + tracked_keys` 的追踪键合并场景。

## 注意事项

- **Web 版依赖链**：仅链接 `HiBerGUIWebEditor` 的程序才依赖 `WebView2Loader.dll` 与 WebView2 运行时；缺运行时 Web 编辑器显示占位不崩溃（`usingWebView()` 返回 false）。体积对比参考：WebView2Loader.dll 含/不含各约 200KB。
- **演示进程无版本资源**：未调用 `nsum_add_version_info`，EXE 属性里无 VERSIONINFO（区别于产品 EXE）。
- **两个独立真实高亮器**：Qt 版 `CodeEditor` 与 Web 版 `WebCodeEditor` 是**两套独立实现**（非同一渲染内核封装），同一 `ICodeEditor` 接口、同一扩展高亮器接口（`ICodeHighlighter`）共用；宿主编排建议一律走 `createCodeEditor(CodeEditorKind::Qt, parent)` 取接口，便于未来替换实现。
- **选型结论（代码内注释）**：merge 预览已选定 **Qt 版**应用于产品 `ConfigFileEditor`，Web 版仅作对比，勿在产品侧默认依赖 Web 版。
- **`--web` 判定**：`qstrcmp(argv[i], "--web") == 0` 后 `findChild<QTabWidget*>()` 切到索引 1；其他未知参数被静默忽略。

## 相关文档

- [EditorDemo README](README.md) — 说明文档
- HiBerGUILibrary — CodeEditor/MergePreviewDialog/ICodeEditor 组件库
- HiBerGUIWebEditor — WebView2 版编辑器实现
- GUIWorker — 产品侧领域编辑器（消费 CodeEditor 的宿主示例）