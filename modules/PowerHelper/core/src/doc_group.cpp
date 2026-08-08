#include "doc_group.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace PowerHelper {

QVector<DocFileInfo> scanDocGroup(const QString& dir)
{
    QVector<DocFileInfo> out;
    QDir root(dir);
    if (!root.exists())
        return out;
    QDirIterator it(dir, { "*.md" }, QDir::Files,
        QDirIterator::Subdirectories);
    QStringList paths;
    while (it.hasNext()) {
        const QString abs = it.next();
        if (abs.contains(QStringLiteral("/.git/"))
            || abs.contains(QStringLiteral("\\.git\\")))
            continue;
        paths.append(QDir::cleanPath(abs));
    }
    paths.sort();

    const QString rootPath = QDir::cleanPath(root.absolutePath());
    for (const QString& abs : paths) {
        QFile f(abs);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString md = QString::fromUtf8(f.readAll());
        DocFileInfo info;
        info.absPath = abs;
        info.relPath = QDir(rootPath).relativeFilePath(abs);
        info.relPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
        info.toc = extractToc(md);
        bool hasTitle = false;
        for (const TocEntry& e : info.toc) {
            if (e.level == 1) {
                info.title = e.text;
                hasTitle = true;
                break;
            }
        }
        if (!hasTitle)
            info.title = QFileInfo(abs).fileName();
        out.append(info);
    }
    return out;
}

} // namespace PowerHelper
