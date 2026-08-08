#pragma once

#include <QString>
#include <QVector>

#include <markdown_renderer.h>
#include <powerhelper_export.h>

namespace PowerHelper {

struct DocFileInfo {
    QString relPath;   // 相对文档组根, 正斜杠
    QString absPath;
    QString title;     // 首个一级标题或文件名
    QVector<TocEntry> toc;
};

// 扫描目录下所有 .md 文件 (递归, 排序), 填充标题与 TOC
QVector<DocFileInfo> PH_API scanDocGroup(const QString& dir);

} // namespace PowerHelper
