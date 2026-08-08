#pragma once

#include <QString>
#include <QStringList>

namespace PowerHelper {
namespace Bridge {

// 定位外壳 EXE (当前进程 exe 同目录 PowerHelper.exe)
QString findPowerHelperExe();

// 拉起独立阅读器 (文件或目录); 成功返回 true
bool launchReader(const QString& target,
    const QStringList& extraArgs = QStringList());

// 默认文档组目录 = 当前 exe 目录/docs
QString defaultDocsDir();

// 目标类型: 目录 -> 文档组, .md 文件 -> 单文档
enum class TargetKind { File, Dir, Unknown };
TargetKind classifyTarget(const QString& path);

} // namespace Bridge
} // namespace PowerHelper
