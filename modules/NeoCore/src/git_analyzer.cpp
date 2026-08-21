#include "git_analyzer.h"

#include <string>
#include <algorithm>

namespace NeoCore {

std::string AnalyzeGitError(const std::string& stderrOutput)
{
    auto contains = [&](const char* keyword) {
        return stderrOutput.find(keyword) != std::string::npos;
    };

    if (contains("fatal: unable to access") ||
        contains("Could not resolve host") ||
        contains("Failed to connect") ||
        contains("Connection refused") ||
        contains("Connection timed out") ||
        contains("Could not read from remote repository")) {
        return "Git 远程仓库连接失败，请检查网络与仓库地址";
    }

    if (contains("Permission denied") ||
        contains("could not read Username") ||
        contains("Authentication failed")) {
        return "Git 认证失败，请检查凭据";
    }

    if (contains("not a git repository") ||
        contains("does not have any commits yet")) {
        return "Git 仓库未初始化或为空";
    }

    if (contains("fatal: couldn't find remote ref") ||
        contains("Remote branch") && contains("not found")) {
        return "Git 远程分支不存在";
    }

    if (contains("merge conflict") ||
        contains("CONFLICT") ||
        contains("Automatic merge failed")) {
        return "Git 合并冲突，请手动解决";
    }

    if (contains("detected dubious ownership")) {
        return "Git 仓库所有权可疑（未信任的仓库），请先信任该仓库后重试";
    }

    if (contains("fatal: not a valid object name")) {
        return "Git 引用无效，分支或标签可能已删除";
    }

    if (contains("error:") || contains("fatal:")) {
        return "Git 操作失败：" + stderrOutput;
    }

    return stderrOutput.empty() ? "Git 操作成功" : stderrOutput;
}

} // namespace NeoCore
