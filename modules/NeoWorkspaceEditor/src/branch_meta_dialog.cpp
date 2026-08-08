#include "branch_meta_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QRegularExpression>

BranchMetaDialog::BranchMetaDialog(const QString& repoDir, QWidget* parent)
    : QDialog(parent)
    , repoDir_(repoDir)
{
    setWindowTitle("分支属性配置");
    resize(560, 420);

    auto* outer = new QVBoxLayout(this);

    auto* title = new QLabel("配置每个 Git 分支的描述与隐藏属性", this);
    QFont tf = title->font();
    tf.setPointSize(13);
    tf.setBold(true);
    title->setFont(tf);
    outer->addWidget(title);

    infoLabel_ = new QLabel("", this);
    infoLabel_->setWordWrap(true);
    outer->addWidget(infoLabel_);

    auto* row = new QHBoxLayout();

    list_ = new QListWidget(this);
    row->addWidget(list_, 1);

    auto* right = new QVBoxLayout();
    auto* group = new QGroupBox("分支属性", this);
    auto* form = new QFormLayout(group);
    descEdit_ = new QLineEdit(group);
    descEdit_->setPlaceholderText("分支描述, 例如: 纯净生存玩法服");
    hiddenCheck_ = new QCheckBox("隐藏此分支（主程序选择页不可见）", group);
    form->addRow("描述:", descEdit_);
    form->addRow("", hiddenCheck_);
    right->addWidget(group);

    auto* tip = new QLabel("保存时将对每个修改过的分支:\n"
        "checkout → 更新该分支的 workspace.json → 提交并推送", this);
    tip->setWordWrap(true);
    tip->setStyleSheet("color: #888; font-size: 12px;");
    right->addWidget(tip);
    right->addStretch();

    row->addLayout(right, 1);
    outer->addLayout(row);

    saveBtn_ = new QPushButton("保存所有修改", this);
    saveBtn_->setMinimumHeight(36);
    outer->addWidget(saveBtn_);

    connect(list_, &QListWidget::currentRowChanged, this, &BranchMetaDialog::onBranchSelected);
    connect(saveBtn_, &QPushButton::clicked, this, &BranchMetaDialog::onSave);

    refreshList();
}

void BranchMetaDialog::refreshList()
{
    branches_.clear();
    list_->clear();
    currentIndex_ = -1;

    QString out, err;
    if (!runGit({"for-each-ref", "--format=%(refname)", "refs/remotes/origin", "refs/heads"}, &out, &err))
        return;

    QString current, defaultBr;
    runGit({"symbolic-ref", "--short", "refs/remotes/origin/HEAD"}, &defaultBr);
    defaultBr = defaultBr.trimmed();
    if (defaultBr.startsWith("origin/")) defaultBr = defaultBr.mid(7);

    runGit({"rev-parse", "--abbrev-ref", "HEAD"}, &current);
    current = current.trimmed();

    QStringList localBranches;
    QString lo;
    if (runGit({"for-each-ref", "--format=%(refname:short)", "refs/heads"}, &lo))
        localBranches = lo.split('\n', Qt::SkipEmptyParts);

    QStringList names;
    for (const auto& line : out.split('\n', Qt::SkipEmptyParts)) {
        QString ref = line.trimmed();
        QString name;
        if (ref.startsWith("refs/remotes/origin/"))
            name = ref.mid(20);
        else if (ref.startsWith("refs/heads/"))
            name = ref.mid(11);
        else
            continue;
        if (name.isEmpty() || name == "HEAD") continue;
        if (!names.contains(name)) names.append(name);
    }

    for (const auto& name : names) {
        BranchMeta m;
        m.name = name;
        m.isDefault = (name == defaultBr);
        m.current = (name == current);
        m.local = localBranches.contains(name);
        loadMeta(m);
        branches_.push_back(m);

        QString label = name;
        if (m.isDefault) label += "  (默认)";
        if (m.current) label += "  [当前]";
        if (m.hidden) label += "  [隐藏]";
        list_->addItem(label);
    }

    if (!branches_.empty()) {
        list_->setCurrentRow(0);
    } else {
        infoLabel_->setText("未找到任何分支。");
    }
}

bool BranchMetaDialog::loadMeta(BranchMeta& meta)
{
    QString content;
    if (!runGit({"show", meta.name + ":workspace.json"}, &content)) {
        meta.description.clear();
        meta.hidden = false;
        return false;
    }

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    QJsonObject root = doc.object();
    meta.description = root.value("description").toString();
    if (meta.description.isEmpty()) {
        meta.description = root.value("workspace").toObject().value("description").toString();
    }
    meta.hidden = root.value("hidden").toBool(false);
    return true;
}

void BranchMetaDialog::onBranchSelected()
{
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(branches_.size())) return;
    currentIndex_ = row;
    const auto& m = branches_[row];

    descEdit_->setText(m.description);
    hiddenCheck_->setChecked(m.hidden);

    infoLabel_->setText(QString("分支: %1\n描述: %2\n隐藏: %3")
        .arg(m.name,
            m.description.isEmpty() ? "(无)" : m.description,
            m.hidden ? "是" : "否"));
}

void BranchMetaDialog::onSave()
{
    if (currentIndex_ < 0) return;
    if (branches_.empty()) return;

    bool anyChanged = false;
    for (auto& m : branches_) {
        bool metaChanged = false;
        QString desc;
        bool hidden = false;

        if (&m == &branches_[currentIndex_]) {
            desc = descEdit_->text();
            hidden = hiddenCheck_->isChecked();
        } else {
            desc = m.description;
            hidden = m.hidden;
        }

        if (desc != m.description || hidden != m.hidden) {
            m.description = desc;
            m.hidden = hidden;
            metaChanged = true;
        }
        if (!metaChanged) continue;
        anyChanged = true;

        QString out;
        if (!runGit({"show", m.name + ":workspace.json"}, &out)) {
            QMessageBox::warning(this, "跳过分支",
                QString("分支 %1 没有 workspace.json，已跳过。").arg(m.name));
            continue;
        }

        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::warning(this, "跳过分支",
                QString("分支 %1 的 workspace.json 解析失败，已跳过。").arg(m.name));
            continue;
        }
        QJsonObject root = doc.object();
        root["description"] = m.description;
        root["hidden"] = m.hidden;

        QApplication::setOverrideCursor(Qt::WaitCursor);

        QString err;
        QString originalBranch;
        runGit({"rev-parse", "--abbrev-ref", "HEAD"}, &originalBranch);
        originalBranch = originalBranch.trimmed();

        bool ok = runGit({"checkout", "-f", m.name}, nullptr, &err);
        if (ok) {
            QFile f(repoDir_ + "/workspace.json");
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
                f.close();
            }
            runGit({"add", "workspace.json"});
            runGit({"commit", "-m", QString("chore: update branch meta (%1)").arg(m.name)}, nullptr, &err);
            if (m.local) {
                runGit({"push", "origin", m.name}, nullptr, &err);
            }
        }

        if (!originalBranch.isEmpty() && originalBranch != m.name) {
            runGit({"checkout", "-f", originalBranch});
        }
        QApplication::restoreOverrideCursor();

        if (!ok) {
            QMessageBox::warning(this, "保存失败",
                QString("分支 %1 保存失败:\n%2").arg(m.name, err));
        }
    }

    if (!anyChanged) {
        QMessageBox::information(this, "分支属性", "没有检测到任何修改。");
        return;
    }

    QString current;
    runGit({"rev-parse", "--abbrev-ref", "HEAD"}, &current);
    QMessageBox::information(this, "保存完成",
        QString("分支属性已保存并推送。\n当前分支: %1").arg(current.trimmed()));
    refreshList();
}

bool BranchMetaDialog::runGit(const QStringList& args, QString* out, QString* err)
{
    QProcess p;
    p.setWorkingDirectory(repoDir_);
    p.start("git", args);
    if (!p.waitForFinished(30000)) return false;
    if (out) *out = QString::fromUtf8(p.readAllStandardOutput());
    if (err) *err = QString::fromUtf8(p.readAllStandardError());
    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}
