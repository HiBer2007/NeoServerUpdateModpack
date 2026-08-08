#include "extra_info_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>

namespace GUIWorker {

ExtraInfoPage::ExtraInfoPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QString::fromUtf8("\u6269\u5c55\u4fe1\u606f"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    auto* subLabel = new QLabel(
        QString::fromUtf8("\u586b\u5199\u5bfc\u51fa\u9700\u8981\u7684\u6269\u5c55\u4fe1\u606f\uff0c\u5e26 * \u4e3a\u5fc5\u586b\u9879\u3002"), this);
    subLabel->setProperty("extraSubLabel", true);
    layout->addWidget(subLabel);

    layout->addSpacing(8);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    content_ = new QWidget(scroll_);
    scroll_->setWidget(content_);
    layout->addWidget(scroll_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->hide();
    layout->addWidget(statusLabel_);

    applyTheme();
}

bool ExtraInfoPage::loadFormat(const QString& formatId)
{
    formatId_ = formatId;
    fields_.clear();
    inputWidgets_.clear();
    formatName_ = formatId;

    if (auto* oldLayout = content_->layout()) {
        while (oldLayout->count() > 0) {
            auto* item = oldLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete oldLayout;
    }
    groupWidgets_.clear();

    auto* contentLayout = new QVBoxLayout(content_);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(8);
    contentLayout->addStretch();

    QString dir = QApplication::applicationDirPath() + QStringLiteral("/exporters");
    QDir exportersDir(dir);
    if (!exportersDir.exists()) {
        return false;
    }

    QStringList metas = exportersDir.entryList(
        {QStringLiteral("*.meta.json")}, QDir::Files, QDir::Name);
    bool found = false;
    for (const auto& f : metas) {
        QFile file(exportersDir.filePath(f));
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &pe);
        file.close();
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) continue;
        QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("format")).toString() != formatId) continue;
        found = true;
        formatName_ = obj.value(QStringLiteral("format")).toString();
        QJsonArray arr = obj.value(QStringLiteral("fields")).toArray();
        for (const auto& v : arr) {
            QJsonObject fo = v.toObject();
            FieldDef f;
            f.key = fo.value(QStringLiteral("key")).toString();
            f.label = fo.value(QStringLiteral("label")).toString();
            f.group = fo.value(QStringLiteral("group")).toString();
            f.type = fo.value(QStringLiteral("type")).toString();
            f.placeholder = fo.value(QStringLiteral("placeholder")).toString();
            f.required = fo.value(QStringLiteral("required")).toBool(false);
            if (!f.key.isEmpty()) fields_.append(f);
        }
        break;
    }
    if (!found) return false;

    QStringList groupOrder;
    for (const auto& f : fields_) {
        if (!groupOrder.contains(f.group)) groupOrder.append(f.group);
    }

    QWidget* lastGroup = nullptr;
    for (const auto& g : groupOrder) {
        auto* group = new QGroupBox(g.isEmpty() ? QString::fromUtf8("\u901a\u7528\u4fe1\u606f") : g, content_);
        auto* form = new QFormLayout(group);
        form->setContentsMargins(12, 10, 12, 10);
        form->setSpacing(6);
        for (const auto& f : fields_) {
            if (f.group != g) continue;
            auto* w = buildFieldWidget(f);
            if (!w) continue;
            QString labelText = f.label;
            if (f.required) labelText += QStringLiteral(" *");
            auto* label = new QLabel(labelText, group);
            label->setStyleSheet(labelStyle(f.required));
            form->addRow(label, w);
            inputWidgets_[f.key] = w;
            fieldLabels_[f.key] = label;
            if (auto* le = qobject_cast<QLineEdit*>(w)) {
                connect(le, &QLineEdit::textChanged, this, [this, f]() {
                    auto* label = fieldLabels_.value(f.key);
                    if (label) label->setStyleSheet(labelStyle(f.required));
                    if (auto* le = qobject_cast<QLineEdit*>(inputWidgets_.value(f.key))) {
                        le->setStyleSheet(lineEditStyle(false));
                    }
                });
            } else if (auto* te = qobject_cast<QTextEdit*>(w)) {
                connect(te, &QTextEdit::textChanged, this, [this, f]() {
                    auto* label = fieldLabels_.value(f.key);
                    if (label) label->setStyleSheet(labelStyle(f.required));
                    if (auto* te = qobject_cast<QTextEdit*>(inputWidgets_.value(f.key))) {
                        te->setStyleSheet(textEditStyle(false));
                    }
                });
            }
        }
        contentLayout->insertWidget(contentLayout->count() - 1, group);
        groupWidgets_.append(qMakePair(g, group));
        lastGroup = group;
    }

    if (fields_.isEmpty()) {
        return true;
    }

    applyTheme();
    return true;
}

bool ExtraInfoPage::hasFields() const
{
    return !fields_.isEmpty();
}

bool ExtraInfoPage::validate() const
{
    return missingRequired().isEmpty();
}

QStringList ExtraInfoPage::missingRequired() const
{
    QStringList missing;
    for (const auto& f : fields_) {
        if (!f.required) continue;
        auto it = inputWidgets_.find(f.key);
        if (it == inputWidgets_.end()) {
            missing << f.label;
            continue;
        }
        QString val;
        if (auto* le = qobject_cast<QLineEdit*>(it.value())) {
            val = le->text().trimmed();
        } else if (auto* te = qobject_cast<QTextEdit*>(it.value())) {
            val = te->toPlainText().trimmed();
        }
        if (val.isEmpty()) missing << f.label;
    }
    return missing;
}

QMap<QString, QString> ExtraInfoPage::values() const
{
    QMap<QString, QString> result;
    for (const auto& f : fields_) {
        auto it = inputWidgets_.find(f.key);
        if (it == inputWidgets_.end()) continue;
        if (auto* le = qobject_cast<QLineEdit*>(it.value())) {
            result[f.key] = le->text().trimmed();
        } else if (auto* te = qobject_cast<QTextEdit*>(it.value())) {
            result[f.key] = te->toPlainText().trimmed();
        }
    }
    return result;
}

bool ExtraInfoPage::setValue(const QString& key, const QString& value)
{
    auto it = inputWidgets_.find(key);
    if (it == inputWidgets_.end()) return false;
    if (auto* le = qobject_cast<QLineEdit*>(it.value())) {
        le->setText(value);
        return true;
    }
    if (auto* te = qobject_cast<QTextEdit*>(it.value())) {
        te->setPlainText(value);
        return true;
    }
    return false;
}

QString ExtraInfoPage::formatName() const
{
    return formatName_;
}

QWidget* ExtraInfoPage::buildFieldWidget(const FieldDef& f)
{
    if (f.type == QStringLiteral("multiline")) {
        auto* te = new QTextEdit(content_);
        te->setPlaceholderText(f.placeholder);
        te->setMaximumHeight(96);
        return te;
    }
    auto* le = new QLineEdit(content_);
    le->setPlaceholderText(f.placeholder);
    return le;
}

void ExtraInfoPage::markMissing(const QStringList& missing)
{
    for (const auto& f : fields_) {
        if (!f.required) continue;
        bool isMissing = missing.contains(f.label);
        auto* label = fieldLabels_.value(f.key);
        if (label) {
            label->setStyleSheet(labelStyle(f.required) +
                (isMissing ? QStringLiteral("color: #e5534b;") : QString()));
        }
        auto it = inputWidgets_.find(f.key);
        if (it != inputWidgets_.end()) {
            if (auto* le = qobject_cast<QLineEdit*>(it.value())) {
                le->setStyleSheet(lineEditStyle(isMissing));
            } else if (auto* te = qobject_cast<QTextEdit*>(it.value())) {
                te->setStyleSheet(textEditStyle(isMissing));
            }
        }
    }
}

QString ExtraInfoPage::lineEditStyle(bool error) const
{
    return QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 3px 6px; }")
        .arg(darkMode_ ? QStringLiteral("#252528") : QStringLiteral("#ffffff"),
             darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000"),
             error ? QStringLiteral("#e5534b")
                   : darkMode_ ? QStringLiteral("#55585e") : QStringLiteral("#d0d0d0"));
}

QString ExtraInfoPage::textEditStyle(bool error) const
{
    return QStringLiteral(
        "QTextEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 3px; }")
        .arg(darkMode_ ? QStringLiteral("#252528") : QStringLiteral("#ffffff"),
             darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000"),
             error ? QStringLiteral("#e5534b")
                   : darkMode_ ? QStringLiteral("#55585e") : QStringLiteral("#d0d0d0"));
}

QString ExtraInfoPage::labelStyle(bool error) const
{
    return QStringLiteral("color: %1; font-size: 13px;")
        .arg(error ? QStringLiteral("#e5534b")
                   : darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#333"));
}

void ExtraInfoPage::applyTheme()
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    const QString dimColor = darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#666");
    const QString warnColor = darkMode_ ? QStringLiteral("#e0aaff") : QStringLiteral("#e67e22");

    const QList<QLabel*> labels = findChildren<QLabel*>();
    for (auto* label : labels) {
        if (label->property("extraSubLabel").toBool()) {
            label->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(dimColor));
        }
    }

    const QList<QGroupBox*> groups = findChildren<QGroupBox*>();
    for (auto* g : groups) {
        g->setStyleSheet(QStringLiteral(
            "QGroupBox { color: %1; font-weight: bold; border: 1px solid %2; border-radius: 8px; margin-top: 10px; padding-top: 6px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }")
            .arg(darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#333"),
                 darkMode_ ? QStringLiteral("#55585e") : QStringLiteral("#d0d0d0")));
    }

    const QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (auto* e : edits) {
        e->setStyleSheet(lineEditStyle(false));
    }

    const QList<QTextEdit*> textEdits = findChildren<QTextEdit*>();
    for (auto* te : textEdits) {
        te->setStyleSheet(textEditStyle(false));
    }

    for (const auto& f : fields_) {
        auto* label = fieldLabels_.value(f.key);
        if (label) {
            label->setStyleSheet(labelStyle(f.required));
        }
    }

    Q_UNUSED(warnColor);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(warnColor));
}

void ExtraInfoPage::updateLabelColors()
{
    applyTheme();
}

int ExtraInfoPage::contentHeight(int widthHint) const
{
    Q_UNUSED(widthHint);
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (!layout) return 0;

    int h = layout->contentsMargins().top() + layout->contentsMargins().bottom();
    int spacing = layout->spacing();
    if (spacing < 0) spacing = 6;

    int count = layout->count();
    for (int i = 0; i < count; ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (QWidget* w = item->widget()) {
            if (w == scroll_) {
                int inner = 0;
                auto* cl = qobject_cast<QVBoxLayout*>(content_->layout());
                if (cl) {
                    for (int j = 0; j < cl->count(); ++j) {
                        QLayoutItem* citem = cl->itemAt(j);
                        if (QWidget* cw = citem->widget()) {
                            inner += cw->sizeHint().height();
                        } else if (QSpacerItem* sp = citem->spacerItem()) {
                            inner += sp->sizeHint().height();
                        }
                        if (j < cl->count() - 1) inner += cl->spacing();
                    }
                }
                h += inner;
            } else if (w->isVisibleTo(this)) {
                h += w->sizeHint().height();
            }
        } else if (QSpacerItem* sp = item->spacerItem()) {
            h += sp->sizeHint().height();
        }
        if (i < count - 1) h += spacing;
    }
    return h;
}

} // namespace GUIWorker

#include "extra_info_page.moc"
