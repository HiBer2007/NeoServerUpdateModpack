#include "ssh_key_dialog.h"

#include <ssh_key.h>

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

namespace GUIWorker {

SshKeyDialog::SshKeyDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Git SSH \u5bc6\u94a5"));
    setMinimumWidth(620);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    hostEdit_ = new QLineEdit(this);
    hostEdit_->setText(QStringLiteral("github.com"));
    form->addRow(QStringLiteral("\u4e3b\u673a"), hostEdit_);

    auto* pathRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(this);
    pathEdit_->setText(QString::fromStdString(NeoWorkspace::defaultSshKeyPath()));
    auto* browseButton = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), this);
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseButton);
    form->addRow(QStringLiteral("\u79c1\u94a5\u8def\u5f84"), pathRow);

    commentEdit_ = new QLineEdit(this);
    commentEdit_->setPlaceholderText(QStringLiteral("nsum"));
    form->addRow(QStringLiteral("\u5907\u6ce8"), commentEdit_);
    layout->addLayout(form);

    auto* buttonRow = new QHBoxLayout();
    auto* generateButton = new QPushButton(QStringLiteral("\u751f\u6210\u65b0\u5bc6\u94a5"), this);
    auto* copyButton = new QPushButton(QStringLiteral("\u590d\u5236\u516c\u94a5"), this);
    auto* testButton = new QPushButton(QStringLiteral("\u6d4b\u8bd5\u8fde\u63a5"), this);
    auto* configureButton = new QPushButton(QStringLiteral("\u5199\u5165 SSH \u914d\u7f6e"), this);
    buttonRow->addWidget(generateButton);
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(testButton);
    buttonRow->addWidget(configureButton);
    layout->addLayout(buttonRow);

    publicKeyEdit_ = new QPlainTextEdit(this);
    publicKeyEdit_->setReadOnly(true);
    publicKeyEdit_->setPlaceholderText(QStringLiteral("\u516c\u94a5"));
    publicKeyEdit_->setMinimumHeight(90);
    layout->addWidget(publicKeyEdit_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    auto* closeRow = new QHBoxLayout();
    auto* saveButton = new QPushButton(QStringLiteral("\u4fdd\u5b58"), this);
    auto* closeButton = new QPushButton(QStringLiteral("\u5173\u95ed"), this);
    closeRow->addStretch(1);
    closeRow->addWidget(saveButton);
    closeRow->addWidget(closeButton);
    layout->addLayout(closeRow);

    connect(browseButton, &QPushButton::clicked, this, &SshKeyDialog::onBrowse);
    connect(generateButton, &QPushButton::clicked, this, &SshKeyDialog::onGenerate);
    connect(copyButton, &QPushButton::clicked, this, &SshKeyDialog::onCopyPublicKey);
    connect(testButton, &QPushButton::clicked, this, &SshKeyDialog::onTest);
    connect(configureButton, &QPushButton::clicked, this, &SshKeyDialog::onConfigure);
    connect(saveButton, &QPushButton::clicked, this, &SshKeyDialog::onSave);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    refreshKey();
}

void SshKeyDialog::setKeyPath(const QString& path)
{
    if (!path.isEmpty()) pathEdit_->setText(path);
    refreshKey();
}

QString SshKeyDialog::keyPath() const
{
    return pathEdit_->text().trimmed();
}

QString SshKeyDialog::host() const
{
    return hostEdit_->text().trimmed().isEmpty()
        ? QStringLiteral("github.com") : hostEdit_->text().trimmed();
}

void SshKeyDialog::setStatus(const QString& text, bool ok)
{
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(ok ? "color: #2e7d32;" : "color: #c62828;");
}

void SshKeyDialog::refreshKey()
{
    const std::string path = keyPath().toStdString();
    if (path.empty()) return;
    const std::string key = NeoWorkspace::readSshPublicKey(path);
    publicKeyEdit_->setPlainText(QString::fromStdString(key));
}

void SshKeyDialog::onBrowse()
{
    const QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("\u79c1\u94a5\u8def\u5f84"), keyPath());
    if (path.isEmpty()) return;
    pathEdit_->setText(path);
    refreshKey();
}

void SshKeyDialog::onGenerate()
{
    std::string error;
    const std::string path = keyPath().toStdString();
    if (path.empty()) {
        setStatus(QStringLiteral("\u79c1\u94a5\u8def\u5f84\u4e0d\u80fd\u4e3a\u7a7a"), false);
        return;
    }
    if (QFileInfo::exists(QString::fromStdString(path))) {
        const auto answer = QMessageBox::question(this,
            QStringLiteral("SSH"),
            QStringLiteral("\u5bc6\u94a5\u5df2\u5b58\u5728\uff0c\u662f\u5426\u91cd\u65b0\u751f\u6210\uff1f"));
        if (answer != QMessageBox::Yes) return;
        QFile::remove(QString::fromStdString(path));
        QFile::remove(QString::fromStdString(path + ".pub"));
    }
    if (!NeoWorkspace::generateSshKey(path, commentEdit_->text().toStdString(), error)) {
        setStatus(QString::fromStdString(error), false);
        return;
    }
    refreshKey();
    setStatus(QStringLiteral("\u5bc6\u94a5\u5df2\u751f\u6210"), true);
}

void SshKeyDialog::onCopyPublicKey()
{
    const QString key = publicKeyEdit_->toPlainText().trimmed();
    if (key.isEmpty()) {
        setStatus(QStringLiteral("\u516c\u94a5\u4e3a\u7a7a"), false);
        return;
    }
    QGuiApplication::clipboard()->setText(key);
    setStatus(QStringLiteral("\u516c\u94a5\u5df2\u590d\u5236"), true);
}

void SshKeyDialog::onTest()
{
    std::string output;
    std::string error;
    setStatus(QStringLiteral("\u6b63\u5728\u6d4b\u8bd5..."), true);
    QGuiApplication::processEvents();
    const bool ok = NeoWorkspace::testSshConnection(host().toStdString(), std::string(),
        keyPath().toStdString(), output, error);
    if (ok) {
        setStatus(QStringLiteral("\u8fde\u63a5\u6210\u529f: ") + QString::fromStdString(output).trimmed(), true);
    } else {
        setStatus(QStringLiteral("\u8fde\u63a5\u5931\u8d25: ") + QString::fromStdString(error).trimmed(), false);
    }
}

void SshKeyDialog::onConfigure()
{
    std::string error;
    if (!NeoWorkspace::configureSshHost(host().toStdString(), keyPath().toStdString(), error)) {
        setStatus(QString::fromStdString(error), false);
        return;
    }
    onSave();
    setStatus(QStringLiteral("\u5df2\u5199\u5165 SSH \u914d\u7f6e"), true);
}

void SshKeyDialog::onSave()
{
    NeoWorkspace::SshConfig config = NeoWorkspace::loadSshConfig();
    config.host = host().toStdString();
    config.keyPath = keyPath().toStdString();
    config.sshPath = NeoWorkspace::defaultSshPath();
    config.gitSshCommand = NeoWorkspace::effectiveSshCommand(config);
    std::string error;
    if (!NeoWorkspace::saveSshConfig(config, error)) {
        setStatus(QString::fromStdString(error), false);
        return;
    }
    setStatus(QStringLiteral("\u5df2\u4fdd\u5b58"), true);
}

} // namespace GUIWorker
