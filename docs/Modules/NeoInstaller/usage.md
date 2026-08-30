# NeoInstaller 使用文档

## 快速开始

### 构建（独立于主配置）

NeoInstaller **仅通过 `installer-static` 预设独立构建**，不参与 `msvc` 主配置。前提：① 已按 `msvc` 预设构建主项目得到 `build/deploy/`（内嵌源）；② 已安装静态 Qt 到 `H:/Qt-static/6.11.1/msvc2022_64`（本机 2026-08-16 尚未安装，需先装）。

```powershell
$cmake = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
& $cmake --preset installer-static
& $cmake --build build_installer
```

产物：`build_installer/.../NeoInstaller.exe`（Release、静态 Qt、内嵌全部 deploy 文件，约 48MB）。
若 `build/deploy` 不存在，配置期报 `FATAL_ERROR: Deploy dir not found: ... build main project first (msvc preset)`。

### 运行

```powershell
.\NeoInstaller.exe                        # GUI 向导
.\NeoInstaller.exe --with-editor          # GUI 向导，预勾选「工作区编辑器」
.\NeoInstaller.exe --silent --path "D:\NeoServer" --with-editor   # 静默安装
```

## 公共 API

本模块为 EXE 应用；对外 API = 命令行参数（installer_main.cpp）+ 内部类接口（头文件在 `src/`）。

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--silent`, `-s` | 静默安装模式：无 GUI，终端输出进度；`QCoreApplication` + `SilentInstaller::run()`，成功退出码 0、失败 1 |
| `--path <path>` | 安装目标目录；缺省时 GUI 向导用安装路径页，静默模式用默认目录（Windows = `%ProgramFiles%\NeoServer`） |
| `--with-editor` | 安装工作区编辑器组件（多建一个 NeoWorkspaceEditor 快捷方式；Git 变体改选 PortableGit） |
| `--use-system-git` | 优先使用系统 Git（不存在时静默模式继续并警告） |
| `--use-bundled-git` | 强制安装内置 Git，忽略系统 Git |
| `--help`, `-h` | 打印用法并退出 0 |

- Git 默认策略（无 Git 标志）= 自动检测：有系统 Git 用之，否则装内置。
- 未知选项 → stderr 报错 + 打印 help + **exit 1**。
- `--with-editor` 决定 Git 变体：**PortableGit**（编辑器用），否则 **MinGit** —— 均打 64-bit `.zip`/`.7z.exe`。Git 恒装（必装组件），除非 `--use-system-git` 且检测成功。

### InstallerWizard（installer_wizard.h）

```cpp
namespace NeoInstaller {
class InstallerWizard : public QWizard {
public:
    explicit InstallerWizard(QWidget* parent = nullptr);
    ~InstallerWizard();
    void setInstallEditor(bool on);
    bool installEditor() const;
    QString gitVariant() const;          // "portablegit"（勾编辑器）/ "mingit"
    enum PageId { PAGE_WELCOME=0, PAGE_LICENSE, PAGE_INSTALL_PATH,
        PAGE_COMPONENTS, PAGE_GIT_CHOICE, PAGE_GIT_LICENSE,
        PAGE_PROGRESS, PAGE_FINISH };
};
}
```

流程：欢迎 → 许可(GPL v3, `:/license/LICENSE`) → 安装路径（默认 `C:\Program Files\NeoServer`；目录非空警告「安装过程将清空该目录的全部内容」）→ 组件（核心必装，编辑器可选）→ Git 选择（检测系统 Git 则预选使用系统；否则强制内置）→ Git 许可(GPL v2) → 进度（清空目录 → 释放 `:/deploy/` → Git 下载/解压 → 桌面+开始菜单快捷方式 → 写 `install.conf`）→ 完成（失败则红字提示）。

### GitChecker（git_checker.h）

```cpp
namespace NeoInstaller {
class GitChecker {
public:
    GitChecker();
    bool isGitInstalled() const;
    std::string findGitPath();           // PATH 扫描 + 常见安装路径探测
    std::string getGitVersion();
    std::string getDownloadUrl() const;  // 固定返回 "https://git-scm.com/download/win"
    bool isValidGitVersion();            // 版本 >= 2.30 视为有效
};
}
```

### GitDownloader（git_downloader.h）

```cpp
namespace NeoInstaller {
enum class GitVariant { MinGit, PortableGit };

class GitDownloader : public QObject {
public:
    explicit GitDownloader(QObject* parent = nullptr);
    void startDownload(GitVariant variant, const QString& extractDir);
    void cancel();
    bool isRunning() const;

    static bool isSystemGitAvailable();
    static QString systemGitPath();
    static QString systemGitVersion();
    static QString fetchLatestGitUrl(GitVariant variant);

signals:
    void progressChanged(int percent, const QString& status);
    void downloadStarted(const QString& url, qint64 totalBytes);
    void downloadProgress(qint64 received, qint64 total);
    void extractProgress(int current, int total);
    void finished(bool success, const QString& message);
};
}
```

实现要点：`fetchLatestGitUrl` 调 GitHub Releases API（`git-for-windows/git/releases/latest`，User-Agent `NeoInstaller/1.0`，15s 超时，403/限流重试）；下载用 `QNetworkAccessManager`（NoLessSafeRedirect、`Accept: application/octet-stream`，临时文件 `%TEMP%/git_download_<uuid>.zip`）；解压先 `tar -xf`，失败回退 PowerShell `Expand-Archive`。

### SilentInstaller（silent_installer.h）

```cpp
namespace NeoInstaller {
struct InstallProgress {
    int percent;
    std::string currentFile;
    std::string stepDescription;
    bool finished;
    bool error;
    std::string errorMessage;
};

class SilentInstaller {
public:
    enum GitMode { Auto, UseSystem, UseBundled };
    SilentInstaller();
    void setInstallEditor(bool on);
    void setGitMode(GitMode m);
    bool run(const std::string& installPath = "");
};
}
```

流程：建目录 → 释放 `:/deploy/` → Git（Auto=系统有则用否则装；UseSystem=必须系统；UseBundled=恒装）→ 快捷方式（主程序必建，勾编辑器另建 NeoWorkspaceEditor）→ 写 `install.conf` → 返回 `true/false`（main 转 exit 0/1）。`install.conf` 格式：

```ini
# NeoServer Install Configuration
install_path=D:\NeoServer
install_editor=true
use_system_git=false
git_path=D:\NeoServer\tools\git\bin\git.exe
```

## 典型用法

```powershell
# 1. 常规图形安装（默认路径 C:\Program Files\NeoServer）
.\NeoInstaller.exe

# 2. 图形安装 + 编辑器 + 内置 Git（PortableGit）
.\NeoInstaller.exe --with-editor --use-bundled-git

# 3. 脚本化静默安装到指定目录（供 CI/批量部署）
.\NeoInstaller.exe --silent --path "D:\NeoServer" --with-editor
echo $LASTEXITCODE    # 0 = 成功, 1 = 失败

# 4. 查看参数说明
.\NeoInstaller.exe --help
```

## 注意事项

- **独立构建铁律**：NeoInstaller 不在主配置（`msvc` 预设）中编译；改它的代码后必须用 `installer-static` 预设单独构建，且**先构建主项目**得到 `build/deploy`（配置期 `FATAL_ERROR` 即此原因）。
- **静态 Qt 前提**：`installer-static` 要求 `H:/Qt-static/6.11.1/msvc2022_64`（预设注释：本机尚未安装 2026-08-16）；未装静态 Qt 时构建会失败。MSVC 静态模式必须 `qt_import_plugins` 导入 `QWindowsIntegrationPlugin`，否则 EXE 启动即「应用程序无法启动，因为缺少平台插件」。
- **目标目录会被清空**：安装路径非空时（GUI 弹警告确认；静默模式直接 `removeRecursively` 后重建）——切勿指向含数据的目录。
- **Git 下载依赖网络 + GitHub API**：`fetchLatestGitUrl` 走 `api.github.com`（15s 超时、403 限流自动等待重试）；Windows 下实际下载在 GUI 进度页用 PowerShell `Invoke-WebRequest`，静默模式用 `Invoke-RestMethod`（TLS12）。公网不可达时 Git 安装失败——GUI 记录「Git 下载失败」，静默模式仅 `[WARN] Git installation failed` 后**继续完成安装**（不视为致命）。
- **解压**：`.zip` 用 Windows `tar`（失败回退 `Expand-Archive`）；`.7z.exe` 自解压包先剥离 `.exe` 后缀改名 `.7z`，用内嵌 `:/tools/7za.exe x <archive> -o<dir> -y`。
- **Git 布局**：MinGit/PortableGit 解压到 `tools/git`；`install.conf` 写的 `git_path` = `tools\git\bin\git.exe`（MinGit 顶层无 `bin/`，git.exe 实际在 `mingw64/bin/git.exe` 与 `cmd/git.exe`，主程序 `InstallConfig::load` 会跨布局探测，见 AGENTS.md「MinGit 无顶层 bin/」）。
- **更新 deploy 后需重打包**：部署文件以内嵌 qrc 方式固化在 EXE 里，主项目 `neo_deploy` 变更后必须重新构建 NeoInstaller 才会带上新内容（qrc 由 CMake 配置期 `file(GLOB_RECURSE ...)` + `installer.qrc` 生成，配置会缓存，必要时删 `build_installer` 重配）。
- **GUI/静默控制台**：EXE 为 WIN32 子系统，GUI 模式无控制台；`--help` 与静默模式先 `AttachConsole(ATTACH_PARENT_PROCESS)`/`AllocConsole` 再 `freopen("CONOUT$")` 输出。
- **Qt 版本一致**：静态构建用 Qt-static 的 Qt6.11.1，与主构建 Qt 版本一致，避免行为差异。

## 相关文档

- [NeoInstaller README](README.md) — 说明文档
- 主程序 `InstallConfig`（根 `src/install_config.h`）— `install.conf` 读取方
- `docs/deploy/` — 被安装内容（操作指南/formats/CLI/PowerHelper/CrashTracker）
- NeoWorkspaceEditor — 可选安装组件（`--with-editor`）