#pragma once

#include <string>
#include <vector>

namespace NeoCore {

// 构建进度数据结构：由构建引擎（导出插件内部）产出，
// 经 IBuildProgress 接口推送至主程序 UI/终端。
struct BuildProgress {
    std::string stage;
    int percent = 0;
    std::string message;
};

struct BuildResult {
    bool success = false;
    std::string outputDir;
    std::string errorMessage;
    std::vector<std::string> warnings;
    int totalFiles = 0;
    int syncedFiles = 0;
    int failedFiles = 0;
};

// 构建进度接口：主程序（GUI/CLI）实现并注入导出插件内的构建引擎。
// 构建引擎通过此接口驱动主进度条、子进度条与日志输出。
//
// 容错约定（实现方与调用方都必须遵守）：
//   - 调用方允许传入空指针，实现方必须静默降级为无 UI 构建；
//   - 子进度条句柄化管理，handle <= 0 或已移除的句柄必须被静默忽略；
//   - 实现方不得因任何输入抛异常或崩溃。
class IBuildProgress {
public:
    virtual ~IBuildProgress() = default;

    // 主进度条
    virtual void set_main_stage(const std::string& stage) = 0;
    virtual void set_main_progress(int percent) = 0;
    virtual void set_main_message(const std::string& message) = 0;

    // 子进度条（句柄化）：返回新句柄（>0），无效输入静默忽略
    virtual int add_sub_bar(const std::string& label) = 0;
    virtual void remove_sub_bar(int handle) = 0;
    virtual void set_sub_progress(int handle, int percent) = 0;
    virtual void set_sub_info(int handle, const std::string& info) = 0;

    // 日志行（构建页日志面板 / CLI 终端）
    virtual void log(const std::string& line) = 0;

    // 取消检查：返回 true 表示应中止构建
    virtual bool is_cancelled() const = 0;
};

} // namespace NeoCore
