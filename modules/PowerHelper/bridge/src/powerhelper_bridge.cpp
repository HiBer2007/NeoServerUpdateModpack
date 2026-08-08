#include "powerhelper_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace PowerHelper {
namespace Bridge {

QString findPowerHelperExe()
{
    const QString candidate = QCoreApplication::applicationDirPath()
        + QStringLiteral("/PowerHelper.exe");
    return QFileInfo::exists(candidate) ? candidate : QString();
}

bool launchReader(const QString& target, const QStringList& extraArgs)
{
    const QString exe = findPowerHelperExe();
    if (exe.isEmpty())
        return false;
    // 目标(文件/目录)必须是 argv[1] —— PowerHelper GUI 以 argv[1] 为目标;
    // 附带参数(--anchor 等)追加在其后。曾把 target append 在 extraArgs 之后,
    // 导致 --anchor 被当作目标文件, PowerHelper 直接 exit 2, 帮助无法打开 (2026-08-07)
    QStringList args;
    args.append(target);
    args.append(extraArgs);
    return QProcess::startDetached(exe, args);
}

QString defaultDocsDir()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/docs");
}

TargetKind classifyTarget(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.isDir())
        return TargetKind::Dir;
    if (fi.isFile())
        return TargetKind::File;
    return TargetKind::Unknown;
}

} // namespace Bridge
} // namespace PowerHelper
