#include "config_file_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QCheckBox>
#include <QTreeWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QRegularExpression>
#include <QCoreApplication>

#include <algorithm>

#include <PluginLoader.h>
#include <IConfigParser.h>
#include <IConfigEditorExtension.h>
#include <logger.h>

#include <merge_preview_dialog.h>

namespace GUIWorker {

namespace {

// 定位一行内某键的整个键值对列区间 (0-based [start,end))。
// 返回 {-1,-1} 表示该行不含此键。支持 json/yaml/toml/properties/snbt 常见写法:
//   json/snbt: "key": value / key:value    yaml: key: value
//   toml: key = value                      properties: key=value / key: value
// 值截断到行内注释 (# 前) 与尾随逗号/括号。
QPair<int, int> keyValueRange(const QString& line, const QString& key,
    const QString& langId)
{
    const bool quoted = langId.contains(QLatin1String("json"))
        || langId.contains(QLatin1String("snbt"));

    auto tryMatch = [&](const QString& keyToTry) -> QPair<int, int> {
        const QString esc = QRegularExpression::escape(keyToTry);
        QString pat;
        if (quoted) {
            pat = QStringLiteral("(\"?)([esc])(\\1\\s*[:=]\\s*)([^,\\n}\\]]+)");
        } else {
            pat = QStringLiteral("(^|[^\\w.-])([esc])(\\s*[:=]\\s*)([^,\\n}\\]]+)");
        }
        const QRegularExpression re(pat);
        const QRegularExpressionMatch m = re.match(line);
        if (!m.hasMatch()) {
            return { -1, -1 };
        }
        const int start = quoted ? m.capturedStart(1) : m.capturedStart(2);
        int end = m.capturedEnd(4);
        const int hash = line.indexOf(QLatin1String(" #"), start);
        if (hash >= start && hash < end) {
            end = hash;
        }
        while (end > start && line.at(end - 1).isSpace()) {
            --end;
        }
        return { start, end };
    };

    auto r = tryMatch(key);
    if (r.first >= 0) {
        return r;
    }
    const int dot = key.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        QString last = key.mid(dot + 1);
        const int br = last.indexOf(QLatin1Char('['));
        if (br > 0) {
            last = last.left(br);
        }
        if (!last.isEmpty()) {
            r = tryMatch(last);
            if (r.first >= 0) {
                return r;
            }
        }
    }
    return { -1, -1 };
}

// 键行 checkbox 指针存于 item data (void* 存储, 无需 metatype)
QCheckBox* keyCheckboxOf(QListWidgetItem* item)
{
    if (!item) return nullptr;
    return static_cast<QCheckBox*>(item->data(Qt::UserRole).value<void*>());
}

// 添加键行: checkbox + 键名 (键名存 item data, 不设 item 文本避免双重渲染)
void addKeyRow(QListWidget* list, const QString& key, bool checked)
{
    auto* item = new QListWidgetItem(list);
    item->setData(Qt::UserRole + 1, key);
    auto* w = new QWidget(list);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(4, 1, 4, 1);
    hl->setSpacing(6);
    auto* cb = new QCheckBox(w);
    cb->setChecked(checked);
    auto* label = new QLabel(key, w);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(cb);
    hl->addWidget(label, 1);
    list->setItemWidget(item, w);
    item->setData(Qt::UserRole, QVariant::fromValue(static_cast<void*>(cb)));
}

// 收集勾选的键 (按当前列表顺序)
QStringList checkedKeysOf(QListWidget* list)
{
    QStringList out;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        auto* cb = keyCheckboxOf(item);
        if (cb && cb->isChecked()) {
            out << item->data(Qt::UserRole + 1).toString();
        }
    }
    return out;
}

// 全选/反选/全不选
void setAllKeyChecks(QListWidget* list, int mode)
{
    for (int i = 0; i < list->count(); ++i) {
        auto* cb = keyCheckboxOf(list->item(i));
        if (!cb) continue;
        if (mode == 0) cb->setChecked(true);
        else if (mode == 1) cb->setChecked(!cb->isChecked());
        else cb->setChecked(false);
    }
}

} // namespace

ConfigFileEditor::ConfigFileEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\u914d\u7f6e\u6587\u4ef6\u540c\u6b65"), this);
    title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #e8eaed;"));

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #ffd54f; font-size: 11px;"));

    parserLabel_ = new QLabel(this);
    parserLabel_->setStyleSheet(QStringLiteral("color: #81c784; font-size: 11px;"));

    lay->addWidget(title);
    lay->addWidget(pathLabel_);
    lay->addWidget(stateLabel_);
    lay->addWidget(parserLabel_);

    auto* modeGroup = new QGroupBox(QString::fromUtf8("\u540c\u6b65\u6a21\u5f0f"), this);
    auto* modeLay = new QVBoxLayout(modeGroup);
    modeLay->setSpacing(6);

    fullRb_ = new QRadioButton(QString::fromUtf8("\u8ddf\u968f\u6587\u4ef6\u5939 (full)\uff1a\u5c0a\u5b88\u6587\u4ef6\u5939\u540c\u6b65\u7b56\u7565"), modeGroup);
    forceRb_ = new QRadioButton(QString::fromUtf8("\u5f3a\u5236\u8986\u76d6 (force)\uff1a\u76f4\u63a5\u5168\u91cf\u5199\u5165\u76ee\u6807"), modeGroup);
    partialRb_ = new QRadioButton(QString::fromUtf8("\u90e8\u5206\u540c\u6b65 (partial)\uff1a\u53ea\u66f4\u65b0\u8ffd\u8e2a\u7684\u952e/\u884c\uff0c\u4fdd\u7559\u76ee\u6807\u672c\u5730\u5185\u5bb9"), modeGroup);
    ignoreRb_ = new QRadioButton(QString::fromUtf8("\u5ffd\u7565 (ignore)\uff1a\u4e0d\u5199\u76ee\u6807"), modeGroup);

    auto* modeBtn = new QButtonGroup(this);
    modeBtn->addButton(fullRb_, 0);
    modeBtn->addButton(forceRb_, 1);
    modeBtn->addButton(partialRb_, 2);
    modeBtn->addButton(ignoreRb_, 3);

    modeLay->addWidget(fullRb_);
    modeLay->addWidget(forceRb_);
    modeLay->addWidget(partialRb_);
    modeLay->addWidget(ignoreRb_);
    lay->addWidget(modeGroup);

    auto* trackedGroup = new QGroupBox(QString::fromUtf8("\u8ffd\u8e2a\u7684\u952e (partial)"), this);
    auto* trackedLay = new QVBoxLayout(trackedGroup);

    keysList_ = new QListWidget(trackedGroup);
    keysList_->setAlternatingRowColors(true);
    trackedLay->addWidget(keysList_, 1);

    auto* selRow = new QHBoxLayout;
    auto* allBtn = new QPushButton(QString::fromUtf8("\u5168\u9009"), trackedGroup);
    auto* invertBtn = new QPushButton(QString::fromUtf8("\u53cd\u9009"), trackedGroup);
    auto* noneBtn = new QPushButton(QString::fromUtf8("\u5168\u4e0d\u9009"), trackedGroup);
    selRow->addWidget(allBtn);
    selRow->addWidget(invertBtn);
    selRow->addWidget(noneBtn);
    selRow->addStretch(1);
    trackedLay->addLayout(selRow);

    connect(allBtn, &QPushButton::clicked, this,
        [this]() { setAllKeyChecks(keysList_, 0); });
    connect(invertBtn, &QPushButton::clicked, this,
        [this]() { setAllKeyChecks(keysList_, 1); });
    connect(noneBtn, &QPushButton::clicked, this,
        [this]() { setAllKeyChecks(keysList_, 2); });

    auto* linesRow = new QHBoxLayout;
    auto* linesLabel = new QLabel(QString::fromUtf8("\u8ffd\u8e2a\u7684\u884c\u53f7 (\u9017\u53f7\u5206\u9694):"), trackedGroup);
    linesEdit_ = new QLineEdit(trackedGroup);
    linesEdit_->setPlaceholderText(QStringLiteral("1,5,9"));
    linesRow->addWidget(linesLabel);
    linesRow->addWidget(linesEdit_, 1);
    trackedLay->addLayout(linesRow);
    lay->addWidget(trackedGroup, 1);

    auto* scopeBox = new QGroupBox(QString::fromUtf8("\u4fdd\u5b58\u8303\u56f4"), this);
    auto* scopeLay = new QVBoxLayout(scopeBox);
    topRb_ = new QRadioButton(QString::fromUtf8("\u9876\u5c42\u9ed8\u8ba4\uff08\u5168\u90e8\u5206\u652f\uff09"), scopeBox);
    branchRb_ = new QRadioButton(QString::fromUtf8("\u5f53\u524d\u5206\u652f\u8986\u76d6"), scopeBox);
    branchRb_->setChecked(true);
    scopeLay->addWidget(topRb_);
    scopeLay->addWidget(branchRb_);
    lay->addWidget(scopeBox);

    auto* btnRow = new QHBoxLayout;
    previewButton_ = new QPushButton(QString::fromUtf8("\u9884\u89c8 merge \u7ed3\u679c"), this);
    saveButton_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58"), this);
    btnRow->addWidget(previewButton_);
    btnRow->addStretch(1);
    btnRow->addWidget(saveButton_);
    lay->addLayout(btnRow);

    connect(modeBtn, &QButtonGroup::idClicked, this, [this](int) {
        updateEnabled();
    });
    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        std::vector<std::string> keys;
        for (const QString& k : checkedKeysOf(keysList_)) {
            keys.push_back(k.toStdString());
        }
        std::vector<int> lines;
        const QStringList parts = linesEdit_->text().split(
            QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
        for (const auto& p : parts) {
            bool ok = false;
            const int v = p.toInt(&ok);
            if (ok && v > 0) lines.push_back(v);
        }
        QString mode;
        if (forceRb_->isChecked()) mode = QStringLiteral("force");
        else if (partialRb_->isChecked()) mode = QStringLiteral("partial");
        else if (ignoreRb_->isChecked()) mode = QStringLiteral("ignore");
        else mode = QStringLiteral("full");
        emit saveRequested(relativePath_, mode, keys, lines, branchRb_->isChecked());
        emit contentModified();
    });
    connect(previewButton_, &QPushButton::clicked, this,
        &ConfigFileEditor::doPreview);

    updateEnabled();
    applyStyle();
}

ConfigFileEditor::~ConfigFileEditor() = default;

void ConfigFileEditor::load(const QString& relativePath,
    const QString& absRepoPath, const QString& repoDir,
    const QString& branch, const QString& effectiveMode,
    const std::vector<std::string>& effectiveKeys,
    const std::vector<int>& effectiveLines,
    bool branchOverrides)
{
    relativePath_ = relativePath;
    absRepoPath_ = absRepoPath;
    repoDir_ = repoDir;
    branch_ = branch;

    pathLabel_->setText(relativePath);
    stateLabel_->setText(branchOverrides
        ? QString::fromUtf8("\u2714 \u5f53\u524d\u5206\u652f\u5df2\u8986\u76d6\u9876\u5c42\u8bbe\u7f6e")
        : QString::fromUtf8("\u2718 \u7ee7\u627f\u9876\u5c42\u8bbe\u7f6e\uff08\u672a\u5728\u672c\u5206\u652f\u8986\u76d6\uff09"));

    if (!loader_) {
        loader_ = std::make_unique<NeoCore::PluginLoader>();
        loader_->ScanDirectory(
            (QCoreApplication::applicationDirPath()
                + QStringLiteral("/parsers")).toStdString());
    }
    parser_ = loader_ ? loader_->FindParser(absRepoPath.toStdString()) : nullptr;
    if (parser_) {
        const auto cap = parser_->capability();
        parserLabel_->setText(QString::fromUtf8("\u2705 \u8bc6\u522b\u89e3\u6790\u5668: %1")
            .arg(QString::fromStdString(cap.name)));
    } else {
        parserLabel_->setText(QString::fromUtf8("\u26a0 \u672a\u627e\u5230\u5bf9\u5e94\u89e3\u6790\u5668\uff0cpartial \u5c06\u56de\u9000\u4e3a\u8986\u76d6\u5e76\u63d0\u793a"));
    }

    // 默认行为: 全量模式 (full); 用户切换到 partial 时默认全部不追踪
    if (effectiveMode == QLatin1String("force")) forceRb_->setChecked(true);
    else if (effectiveMode == QLatin1String("partial")) partialRb_->setChecked(true);
    else if (effectiveMode == QLatin1String("ignore")) ignoreRb_->setChecked(true);
    else if (effectiveMode == QLatin1String("full")) fullRb_->setChecked(true);
    else fullRb_->setChecked(true);

    keysList_->clear();
    if (parser_) {
        reloadKeys();
        // 仅勾选规则中明确列出的键; 无规则/未指定 = 全部不追踪
        for (int i = 0; i < keysList_->count(); ++i) {
            auto* item = keysList_->item(i);
            auto* cb = keyCheckboxOf(item);
            if (!cb) continue;
            const std::string key = item->data(Qt::UserRole + 1)
                .toString().toStdString();
            cb->setChecked(
                std::find(effectiveKeys.begin(), effectiveKeys.end(), key)
                    != effectiveKeys.end());
        }
        // 切换文件/分支后强制重排重绘, 避免子项渲染不全
        keysList_->doItemsLayout();
        keysList_->viewport()->update();
    }

    QStringList lineParts;
    for (int v : effectiveLines) {
        lineParts << QString::number(v);
    }
    linesEdit_->setText(lineParts.join(QStringLiteral(", ")));

    updateEnabled();
}

void ConfigFileEditor::setScopeTop()
{
    topRb_->setChecked(true);
}

void ConfigFileEditor::setEditorExtension(NeoCore::IConfigEditorExtension* ext)
{
    editorExt_ = ext;
}

void ConfigFileEditor::reloadKeys()
{
    keysList_->clear();
    if (!parser_ || absRepoPath_.isEmpty()) return;
    std::vector<std::string> keys;
    try {
        keys = parser_->list_keys(absRepoPath_.toStdString());
    } catch (const std::exception& e) {
        CLogger::Error("ConfigFileEditor: list_keys exception: {}", e.what());
        return;
    }
    for (const auto& k : keys) {
        addKeyRow(keysList_, QString::fromStdString(k), false);
    }
    keysList_->doItemsLayout();
    keysList_->viewport()->update();
}

void ConfigFileEditor::updateEnabled()
{
    const bool partial = partialRb_->isChecked();
    keysList_->setEnabled(partial);
    linesEdit_->setEnabled(partial);
}

void ConfigFileEditor::doPreview()
{
    if (!parser_) {
        QMessageBox::information(this,
            QString::fromUtf8("\u9884\u89c8"),
            QString::fromUtf8("\u6ca1\u6709\u53ef\u7528\u7684\u89e3\u6790\u5668\uff0c\u65e0\u6cd5\u751f\u6210 merge \u9884\u89c8\u3002"));
        return;
    }

    // 当前同步模式
    const QString mode = fullRb_->isChecked()
        ? QStringLiteral("full")
        : (forceRb_->isChecked() ? QStringLiteral("force")
            : (partialRb_->isChecked() ? QStringLiteral("partial")
                : QStringLiteral("ignore")));

    std::vector<std::string> keys;
    for (const QString& k : checkedKeysOf(keysList_)) {
        keys.push_back(k.toStdString());
    }

    QFile remoteFile(absRepoPath_);
    if (!remoteFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QString::fromUtf8("\u9884\u89c8"),
            QString::fromUtf8("\u65e0\u6cd5\u8bfb\u53d6\u6e90\u6587\u4ef6:\n%1").arg(absRepoPath_));
        return;
    }
    const QString remote = QString::fromUtf8(remoteFile.readAll());
    remoteFile.close();

    const QString langId = [&]() {
        const QString lower = relativePath_.toLower();
        if (lower.endsWith(QStringLiteral(".properties"))) {
            return QStringLiteral("properties");
        }
        if (lower.endsWith(QStringLiteral(".json"))) {
            return QStringLiteral("json");
        }
        if (lower.endsWith(QStringLiteral(".yaml"))
            || lower.endsWith(QStringLiteral(".yml"))) {
            return QStringLiteral("yaml");
        }
        if (lower.endsWith(QStringLiteral(".toml"))) {
            return QStringLiteral("toml");
        }
        if (lower.endsWith(QStringLiteral(".snbt"))) {
            return QStringLiteral("snbt");
        }
        return QStringLiteral("txt");
    }();

    // 预览语义 = 标记哪些项会被合并 (不模拟合并内容):
    //   full    全部行黄 (来自上层文件夹策略的覆盖控制)
    //   force   全部行绿 (强制全量覆盖)
    //   partial 追踪键的键值对绿 (未选键则无标记)
    //   ignore  无标记 (不写目标)
    QVector<HiBerGUI::RegionHighlight> highlights;
    QString info;
    const QStringList rl = remote.split(QLatin1Char('\n'));

    if (mode == QStringLiteral("full")) {
        for (int i = 0; i < rl.size(); ++i) {
            highlights << HiBerGUI::RegionHighlight{ i + 1, i + 1,
                QStringLiteral("#8a6d1a"), QStringLiteral("overwrite"),
                -1, -1, QStringLiteral("#f7e8a8") };
        }
        info = QString::fromUtf8(
            "\u9ec4\u8272\u6807\u8bb0 = \u6765\u81ea\u4e0a\u5c42\u6587\u4ef6\u5939\u7b56\u7565\u7684\u8986\u76d6\u63a7\u5236\uff08%1 \u884c\u5168\u90e8\u8986\u76d6\uff09")
            .arg(rl.size());
    } else if (mode == QStringLiteral("force")) {
        for (int i = 0; i < rl.size(); ++i) {
            highlights << HiBerGUI::RegionHighlight{ i + 1, i + 1,
                QStringLiteral("#2f6b31"), QStringLiteral("tracked") };
        }
        info = QString::fromUtf8(
            "\u7eff\u8272\u6807\u8bb0 = \u5f3a\u5236\u8986\u76d6\u7684\u884c\uff08%1 \u884c\u5168\u90e8\u8986\u76d6\uff09")
            .arg(rl.size());
    } else if (mode == QStringLiteral("partial")) {
        if (keys.empty()) {
            info = QString::fromUtf8(
                "\u672a\u9009\u62e9\u8ffd\u8e2a\u952e\uff0c\u76ee\u6807\u4fdd\u6301\u4e0d\u53d8\u3002");
        } else {
            int marked = 0;
            if (editorExt_) {
                // 编辑插件执行键值对定位: 返回追踪键所在行列表 (1-based),
                // 生成整行标记; 插件未命中时该行不标记
                const auto lines = editorExt_->trackedLines(
                    remote.toStdString(), keys);
                for (int ln : lines) {
                    if (ln < 1 || ln > rl.size()) continue;
                    highlights << HiBerGUI::RegionHighlight{ ln, ln,
                        QStringLiteral("#2f6b31"), QStringLiteral("tracked") };
                    ++marked;
                }
            } else {
                // 无插件回退: 行内键值对正则定位
                for (int i = 0; i < rl.size(); ++i) {
                    for (const std::string& k : keys) {
                        const auto range = keyValueRange(rl[i],
                            QString::fromStdString(k), langId);
                        if (range.first >= 0) {
                            highlights << HiBerGUI::RegionHighlight{ i + 1, i + 1,
                                QStringLiteral("#2f6b31"), QStringLiteral("tracked"),
                                range.first, range.second };
                            ++marked;
                        }
                    }
                }
            }
            info = QString::fromUtf8(
                "\u7eff\u8272\u6807\u8bb0 = \u5c06\u5408\u5e76\u7684\u8ffd\u8e2a\u952e\u503c\u5bf9\uff08%1 \u4e2a\u952e\uff09")
                .arg(static_cast<int>(keys.size()));
            Q_UNUSED(marked);
        }
    } else {
        info = QString::fromUtf8(
            "\u6b64\u6587\u4ef6\u5c06\u88ab\u5ffd\u7565\uff0c\u4e0d\u4f1a\u5199\u5165\u76ee\u6807\u3002");
    }

    HiBerGUI::MergePreviewDialog dialog(HiBerGUI::CodeEditorKind::Qt, this);
    dialog.setContent(remote, info, langId, highlights);
    dialog.setWindowTitle(QString::fromUtf8("merge \u9884\u89c8 - %1").arg(relativePath_));
    dialog.resize(680, 520);
    dialog.exec();
}

void ConfigFileEditor::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QGroupBox {
            color: #c8ccd2;
            border: 1px solid #454b54;
            border-radius: 6px;
            margin-top: 8px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QRadioButton {
            color: #d8dce2;
            spacing: 6px;
        }
        QListWidget {
            background-color: #262a30;
            color: #d8dce2;
            border: 1px solid #3a4048;
            border-radius: 4px;
        }
        QLineEdit {
            background-color: #262a30;
            color: #d8dce2;
            border: 1px solid #3a4048;
            border-radius: 4px;
            padding: 3px 6px;
        }
        QPushButton {
            background-color: #3a6ea5;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #4682b8;
        }
    )"));
}

} // namespace GUIWorker

#include "config_file_editor.moc"
