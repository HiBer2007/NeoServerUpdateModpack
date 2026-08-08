#include "export_dir_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QColor>
#include <QPalette>

namespace GUIWorker {

ExportDirPage::ExportDirPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55"), this);
    titleLabel_ = titleLabel;
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    auto* subLabel = new QLabel(
        QStringLiteral("\u9009\u62e9\u6574\u5408\u5305\u5bfc\u51fa\u7684\u4fdd\u5b58\u76ee\u5f55\uff0c\u6587\u4ef6\u540d\u5c06\u81ea\u52a8\u751f\u6210\u3002"), this);
    subLabel_ = subLabel;
    subLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 12px;"));
    layout->addWidget(subLabel);

    layout->addSpacing(10);

    auto* row = new QHBoxLayout();
    dirEdit_ = new QLineEdit(this);
    dirEdit_->setPlaceholderText(QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55..."));
    dirEdit_->setMinimumHeight(32);
    browseBtn_ = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), this);
    browseBtn_->setMinimumHeight(32);
    row->addWidget(dirEdit_, 1);
    row->addWidget(browseBtn_);
    layout->addLayout(row);

    layout->addStretch();

    connect(browseBtn_, &QPushButton::clicked, this, &ExportDirPage::onBrowse);
    connect(dirEdit_, &QLineEdit::textChanged, this, &ExportDirPage::onPathChanged);
}

void ExportDirPage::setContext(const QString& modpackBranch, const QString& formatId,
                               const QString& extension)
{
    modpackBranch_ = modpackBranch;
    formatId_ = formatId;
    extension_ = extension;
    directoryMode_ = (formatId == QLatin1String("hmcl"));

    if (directoryMode_) {
        titleLabel_->setText(QStringLiteral("\u9009\u62e9\u76ee\u6807\u5de5\u4f5c\u76ee\u5f55"));
        subLabel_->setText(QStringLiteral(
            "\u9009\u62e9 HMCL \u6e38\u620f\u5de5\u4f5c\u76ee\u5f55\uff08\u5982 .minecraft\\versions\\\u6574\u5408\u5305\u540d\uff09\uff0c\u6784\u5efa\u7ed3\u679c\u5c06\u771f\u5b9e\u5199\u5165\u8be5\u76ee\u5f55\u3002"));
        dirEdit_->setPlaceholderText(QStringLiteral("\u9009\u62e9\u76ee\u6807\u5de5\u4f5c\u76ee\u5f55..."));
    } else {
        titleLabel_->setText(QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55"));
        subLabel_->setText(QStringLiteral(
            "\u9009\u62e9\u6574\u5408\u5305\u5bfc\u51fa\u7684\u4fdd\u5b58\u76ee\u5f55\uff0c\u6587\u4ef6\u540d\u5c06\u81ea\u52a8\u751f\u6210\u3002"));
        dirEdit_->setPlaceholderText(QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55..."));
    }
}

QString ExportDirPage::outputPath() const
{
    QString dir = dirEdit_->text().trimmed();
    if (dir.isEmpty()) return QString();
    if (directoryMode_) {
        return QDir::toNativeSeparators(dir);
    }
    QString base = modpackBranch_.isEmpty() ? QStringLiteral("modpack") : modpackBranch_;
    return QDir::toNativeSeparators(dir + QStringLiteral("/") + base + QStringLiteral("_modpack") + extension_);
}

void ExportDirPage::setPath(const QString& path)
{
    dirEdit_->setText(path);
}

bool ExportDirPage::isValid() const
{
    return !dirEdit_->text().trimmed().isEmpty();
}

void ExportDirPage::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55"),
        dirEdit_->text().trimmed(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        dirEdit_->setText(QDir::toNativeSeparators(dir));
        emit dirSelected(outputPath());
    }
}

void ExportDirPage::onPathChanged(const QString&)
{
}

} // namespace GUIWorker

#include "export_dir_page.moc"