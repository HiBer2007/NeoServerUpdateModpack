#pragma once

#include <QString>
#include <QPair>
#include <QList>

namespace GUIWorker {

// 下拉显示映射: (显示文本[中文+解释], 存储值[英文 ID])
// 显示给用户看中文，保存/加载始终用英文 ID 与配置文件交互。

QList<QPair<QString, QString>> folderPolicyDisplayItems();
QList<QPair<QString, QString>> fileModeDisplayItems();
QList<QPair<QString, QString>> serverConfigModeDisplayItems();

} // namespace GUIWorker
