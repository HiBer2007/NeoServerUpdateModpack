#include "done_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

#include <powerhelper_bridge.h>

namespace GUIWorker {

DonePage::DonePage(QWidget* parent)
    : QWidget(parent)
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(12);

    iconLabel_ = new QLabel(this);
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setStyleSheet(QStringLiteral("font-size: 42px;"));
    layout->addWidget(iconLabel_);

    titleLabel_ = new QLabel(this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setStyleSheet(QStringLiteral("color: %1;")
        .arg(darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000")));
    layout->addWidget(titleLabel_);

    messageLabel_ = new QLabel(this);
    messageLabel_->setAlignment(Qt::AlignCenter);
    messageLabel_->setWordWrap(true);
    messageLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
        .arg(darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    layout->addWidget(messageLabel_);

    warningLabel_ = new QLabel(this);
    warningLabel_->setAlignment(Qt::AlignCenter);
    warningLabel_->setStyleSheet(QStringLiteral("color: #d8b48a; font-size: 12px;"));
    warningLabel_->hide();
    layout->addWidget(warningLabel_);

    warnDetailsBtn_ = new QPushButton(
        QString::fromUtf8("\u5c55\u5f00\u8b66\u544a\u8be6\u60c5"), this);
    warnDetailsBtn_->setMinimumHeight(30);
    warnDetailsBtn_->setMinimumWidth(140);
    warnDetailsBtn_->hide();
    layout->addWidget(warnDetailsBtn_, 0, Qt::AlignHCenter);
    connect(warnDetailsBtn_, &QPushButton::clicked,
        this, &DonePage::onShowWarningDetails);

    suggestionLabel_ = new QLabel(this);
    suggestionLabel_->setAlignment(Qt::AlignCenter);
    suggestionLabel_->setWordWrap(true);
    suggestionLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode_ ? QStringLiteral("#d8b48a") : QStringLiteral("#b06000")));
    suggestionLabel_->hide();
    layout->addWidget(suggestionLabel_);

    layout->addSpacing(10);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    helpBtn_ = new QPushButton(
        QString::fromUtf8("\u6253\u5f00\u5e2e\u52a9\u6587\u6863"), this);
    helpBtn_->setMinimumHeight(34);
    helpBtn_->setMinimumWidth(120);
    helpBtn_->hide();
    btnRow->addWidget(helpBtn_);

    openDirBtn_ = new QPushButton(QString::fromUtf8("\u6253\u5f00\u8f93\u51fa\u76ee\u5f55"), this);
    openDirBtn_->setMinimumHeight(34);
    openDirBtn_->setMinimumWidth(120);
    btnRow->addWidget(openDirBtn_);

    exitBtn_ = new QPushButton(QString::fromUtf8("\u9000\u51fa"), this);
    exitBtn_->setMinimumHeight(34);
    exitBtn_->setMinimumWidth(120);
    btnRow->addWidget(exitBtn_);

    btnRow->addStretch();
    layout->addLayout(btnRow);

    layout->addStretch();

    connect(openDirBtn_, &QPushButton::clicked, this, [this]() {
        if (!outputDir_.isEmpty()) {
            emit openOutputDirRequested(outputDir_);
        }
    });
    connect(helpBtn_, &QPushButton::clicked, this, []() {
        const QString docs = PowerHelper::Bridge::defaultDocsDir();
        if (!PowerHelper::Bridge::launchReader(docs)) {
            QMessageBox::warning(nullptr,
                QString::fromUtf8("\u65e0\u6cd5\u6253\u5f00\u5e2e\u52a9"),
                QString::fromUtf8(
                    "\u672a\u627e\u5230 PowerHelper.exe\uff0c\u65e0\u6cd5\u6253\u5f00\u5e2e\u52a9\u6587\u6863\u3002"));
        }
    });
    connect(exitBtn_, &QPushButton::clicked, this, &DonePage::finishRequested);
}

void DonePage::showSuccess(const QString& outputDir, const QStringList& warnings)
{
    outputDir_ = outputDir;
    warnings_ = warnings;
    iconLabel_->setText(QStringLiteral("\u2714"));
    iconLabel_->setStyleSheet(QStringLiteral("font-size: 42px; color: #3fb950;"));
    titleLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5b8c\u6210"));
    messageLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5df2\u5b8c\u6210\uff0c\u8f93\u51fa\u76ee\u5f55:\n%1")
        .arg(outputDir));
    suggestionLabel_->hide();

    if (warnings_.isEmpty()) {
        warningLabel_->hide();
        warnDetailsBtn_->hide();
        helpBtn_->hide();
    } else {
        warningLabel_->setText(
            QString::fromUtf8("\u5e76\u4f34\u968f %1 \u4e2a\u8b66\u544a")
                .arg(warnings_.size()));
        warningLabel_->show();
        warnDetailsBtn_->show();
        helpBtn_->show();
    }
    openDirBtn_->show();
}

void DonePage::showFailure(const QString& reason, const QString& suggestion)
{
    outputDir_.clear();
    warnings_.clear();
    iconLabel_->setText(QStringLiteral("\u2716"));
    iconLabel_->setStyleSheet(QStringLiteral("font-size: 42px; color: #e5534b;"));
    titleLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5931\u8d25"));
    messageLabel_->setText(reason.isEmpty() ? QString::fromUtf8("\u672a\u77e5\u9519\u8bef\u3002")
                                            : reason);
    if (suggestion.isEmpty()) {
        suggestionLabel_->hide();
    } else {
        suggestionLabel_->setText(QString::fromUtf8("\u53ef\u80fd\u89e3\u51b3\u65b9\u6848:\n%1").arg(suggestion));
        suggestionLabel_->show();
    }
    warningLabel_->hide();
    warnDetailsBtn_->hide();
    helpBtn_->show();
    openDirBtn_->hide();
}

void DonePage::onShowWarningDetails()
{
    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("\u6784\u5efa\u8b66\u544a"));
    box.setIcon(QMessageBox::Warning);
    if (warnings_.isEmpty()) {
        box.setText(QString::fromUtf8("\u65e0\u8b66\u544a\u3002"));
    } else {
        QString text = QString::fromUtf8("\u5171 %1 \u4e2a\u8b66\u544a:\n\n")
            .arg(warnings_.size());
        for (int i = 0; i < warnings_.size(); ++i) {
            text += QStringLiteral("%1. %2\n").arg(i + 1).arg(warnings_.at(i));
        }
        box.setText(text);
    }
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

} // namespace GUIWorker

#include "done_page.moc"
