#include "repo_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QButtonGroup>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QColor>
#include <QPalette>
#include <QSignalBlocker>
#include <algorithm>
#include <crtdbg.h>
#include <logger.h>
#include <history_store.h>

namespace GUIWorker {

RepoPage::RepoPage(QWidget* parent)
    : QWidget(parent)
    , currentType_(SourceRemote)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    CLogger::Info("RepoPage construct: after layout={}", _CrtCheckMemory() ? "OK" : "CORRUPT");

    auto* titleLabel = new QLabel(QStringLiteral("\u9009\u62e9\u4ed3\u5e93\u6e90"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    CLogger::Info("RepoPage construct: after title={}", _CrtCheckMemory() ? "OK" : "CORRUPT");

    layout->addSpacing(4);

    auto* subLabel = new QLabel(
        QStringLiteral("\u9009\u62e9\u4ed3\u5e93\u6765\u6e90\u7c7b\u578b\uff0c\u518d\u8f93\u5165\u5bf9\u5e94\u5730\u5740\u3002"), this);
    subLabel->setProperty("repoSubLabel", true);
    layout->addWidget(subLabel);

    layout->addSpacing(6);

    struct TypeDef {
        QString title;
        QString help;
    };
    const TypeDef types[3] = {
        {
            QStringLiteral("\u514b\u9686\u8fdc\u7a0b\u4ed3\u5e93"),
            QStringLiteral("\u4ece\u8fdc\u7a0b Git \u4ed3\u5e93\u514b\u9686\u6574\u5408\u5305\u3002"
                           "\u652f\u6301 HTTPS\uff08\u4f8b: https://github.com/user/repo.git\uff09\u3001"
                           "SSH\uff08\u4f8b: git@github.com:user/repo.git\uff09\u6216 "
                           "file:// \u672c\u5730\u8def\u5f84\u3002")
        },
        {
            QStringLiteral("\u6253\u5f00\u672c\u5730\u4ed3\u5e93\u76ee\u5f55"),
            QStringLiteral("\u6253\u5f00\u5df2\u514b\u9686\u5728\u672c\u673a\u7684\u4ed3\u5e93\u76ee\u5f55\u3002"
                           "\u8be5\u76ee\u5f55\u5fc5\u987b\u662f\u542b\u6709 .git \u7684 Git \u4ed3\u5e93\uff0c"
                           "\u5e76\u4e14\u6839\u76ee\u5f55\u5305\u542b workspace.json \u3002")
        },
        {
            QStringLiteral("\u6253\u5f00\u8fdc\u7a0b\u4ed3\u5e93\u7684\u672c\u5730\u7f13\u5b58"),
            QStringLiteral("\u6253\u5f00\u8fdc\u7a0b\u4ed3\u5e93\u7684\u672c\u5730\u7f13\u5b58\u76ee\u5f55\u3002"
                           "\u7528\u4e8e\u4e4b\u524d\u6784\u5efa\u8fc7\u7684\u9879\u76ee\uff0c"
                           "\u53ef\u7701\u53bb\u91cd\u590d\u514b\u9686\uff0c\u5feb\u901f\u91cd\u7528\u5df2\u6709\u4ee3\u7801\u3002")
        }
    };

    auto* typeRow = new QHBoxLayout();
    auto* typeGroup = new QButtonGroup(this);
    typeGroup->setExclusive(true);
    for (int i = 0; i < 3; ++i) {
        auto* btn = new QPushButton(types[i].title, this);
        btn->setCheckable(true);
        btn->setMinimumHeight(30);
        btn->setProperty("repoTypeBtn", true);
        btn->setProperty("typeIndex", i);
        typeGroup->addButton(btn, i);
        typeRow->addWidget(btn);
        typeBtns_.push_back(btn);
    }
    layout->addLayout(typeRow);

    layout->addSpacing(6);

    helpLabel_ = new QLabel(types[0].help, this);
    helpLabel_->setWordWrap(true);
    helpLabel_->setProperty("repoHelpBox", true);
    layout->addWidget(helpLabel_);

    layout->addSpacing(6);

    auto* recentLabel = new QLabel(QStringLiteral("\u6700\u8fd1\u4f7f\u7528\u7684\u4ed3\u5e93:"), this);
    recentLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(recentLabel);

    recentCombo_ = new QComboBox(this);
    recentCombo_->setMinimumHeight(30);
    recentCombo_->setMaximumHeight(30);
    recentCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    recentCombo_->setMinimumContentsLength(24);
    layout->addWidget(recentCombo_);

    layout->addSpacing(6);

    inputStack_ = new QStackedWidget(this);

    auto* remotePage = new QWidget(this);
    auto* remoteLayout = new QVBoxLayout(remotePage);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    remoteEdit_ = new QLineEdit(remotePage);
    remoteEdit_->setPlaceholderText(
        QStringLiteral("\u8f93\u5165 Git \u4ed3\u5e93\u5730\u5740 (\u4f8b\u5982: https://github.com/user/repo.git)"));
    remoteEdit_->setMinimumHeight(30);
    remoteLayout->addWidget(remoteEdit_);
    inputStack_->addWidget(remotePage);

    auto* localPage = new QWidget(this);
    auto* localLayout = new QVBoxLayout(localPage);
    localLayout->setContentsMargins(0, 0, 0, 0);
    auto* localRow = new QHBoxLayout();
    localEdit_ = new QLineEdit(localPage);
    localEdit_->setPlaceholderText(QStringLiteral("\u9009\u62e9\u672c\u5730\u4ed3\u5e93\u76ee\u5f55..."));
    localEdit_->setMinimumHeight(30);
    localBrowse_ = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), localPage);
    localBrowse_->setMinimumHeight(30);
    localRow->addWidget(localEdit_, 1);
    localRow->addWidget(localBrowse_);
    localLayout->addLayout(localRow);
    inputStack_->addWidget(localPage);

    auto* cachePage = new QWidget(this);
    auto* cacheLayout = new QVBoxLayout(cachePage);
    cacheLayout->setContentsMargins(0, 0, 0, 0);
    auto* cacheRow = new QHBoxLayout();
    cacheEdit_ = new QLineEdit(cachePage);
    cacheEdit_->setPlaceholderText(QStringLiteral("\u9009\u62e9\u672c\u5730\u7f13\u5b58\u76ee\u5f55..."));
    cacheEdit_->setMinimumHeight(30);
    cacheBrowse_ = new QPushButton(QStringLiteral("\u6d4f\u89c8..."), cachePage);
    cacheBrowse_->setMinimumHeight(30);
    cacheRow->addWidget(cacheEdit_, 1);
    cacheRow->addWidget(cacheBrowse_);
    cacheLayout->addLayout(cacheRow);
    inputStack_->addWidget(cachePage);

    layout->addWidget(inputStack_);

    hintLabel_ = new QLabel(QStringLiteral(""), this);
    hintLabel_->setProperty("repoHintLabel", true);
    hintLabel_->setWordWrap(true);
    hintLabel_->hide();
    layout->addWidget(hintLabel_);

    layout->addStretch();

    connect(typeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
        this, [this](int id) {
            currentType_ = id;
            onTypeChanged();
            loadRecentRepos(true);
        });
    connect(localBrowse_, &QPushButton::clicked, this, &RepoPage::onBrowseLocal);
    connect(cacheBrowse_, &QPushButton::clicked, this, &RepoPage::onBrowseCache);
    connect(remoteEdit_, &QLineEdit::textChanged, this, &RepoPage::onUrlChanged);
    connect(localEdit_, &QLineEdit::textChanged, this, &RepoPage::onUrlChanged);
    connect(cacheEdit_, &QLineEdit::textChanged, this, &RepoPage::onUrlChanged);
    connect(remoteEdit_, &QLineEdit::returnPressed, this, &RepoPage::onUrlReturnPressed);
    connect(localEdit_, &QLineEdit::returnPressed, this, &RepoPage::onUrlReturnPressed);
    connect(cacheEdit_, &QLineEdit::returnPressed, this, &RepoPage::onUrlReturnPressed);
    connect(recentCombo_, QOverload<int>::of(&QComboBox::activated),
        this, &RepoPage::onRecentSelected);

    typeBtns_[0]->setChecked(true);
    CLogger::Info("RepoPage construct: after setChecked={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    inputStack_->setCurrentIndex(SourceRemote);
    CLogger::Info("RepoPage construct: after setCurrentIndex={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    applyTheme();
    CLogger::Info("RepoPage construct: after applyTheme={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    loadRecentRepos(true);
    CLogger::Info("RepoPage construct: after loadRecentRepos={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
}

QString RepoPage::repoUrl() const
{
    return currentInput().trimmed();
}

void RepoPage::setUrl(const QString& url)
{
    currentType_ = SourceRemote;
    for (auto* b : typeBtns_) {
        const QSignalBlocker blocker(b);
        b->setChecked(false);
    }
    if (static_cast<int>(typeBtns_.size()) > SourceRemote) {
        const QSignalBlocker blocker(typeBtns_[SourceRemote]);
        typeBtns_[SourceRemote]->setChecked(true);
    }
    remoteEdit_->setText(url);
    onTypeChanged();
}

bool RepoPage::isValid() const
{
    return !repoUrl().isEmpty();
}

QString RepoPage::currentInput() const
{
    switch (currentType_) {
    case SourceLocal:  return localEdit_->text();
    case SourceCache:  return cacheEdit_->text();
    case SourceRemote:
    default:           return remoteEdit_->text();
    }
}

void RepoPage::onTypeChanged()
{
    static const QString helps[3] = {
        QStringLiteral("\u4ece\u8fdc\u7a0b Git \u4ed3\u5e93\u514b\u9686\u6574\u5408\u5305\u3002"
                       "\u652f\u6301 HTTPS\uff08\u4f8b: https://github.com/user/repo.git\uff09\u3001"
                       "SSH\uff08\u4f8b: git@github.com:user/repo.git\uff09\u6216 "
                       "file:// \u672c\u5730\u8def\u5f84\u3002"),
        QStringLiteral("\u6253\u5f00\u5df2\u514b\u9686\u5728\u672c\u673a\u7684\u4ed3\u5e93\u76ee\u5f55\u3002"
                       "\u8be5\u76ee\u5f55\u5fc5\u987b\u662f\u542b\u6709 .git \u7684 Git \u4ed3\u5e93\uff0c"
                       "\u5e76\u4e14\u6839\u76ee\u5f55\u5305\u542b workspace.json \u3002"),
        QStringLiteral("\u6253\u5f00\u8fdc\u7a0b\u4ed3\u5e93\u7684\u672c\u5730\u7f13\u5b58\u76ee\u5f55\u3002"
                       "\u7528\u4e8e\u4e4b\u524d\u6784\u5efa\u8fc7\u7684\u9879\u76ee\uff0c"
                       "\u53ef\u7701\u53bb\u91cd\u590d\u514b\u9686\uff0c\u5feb\u901f\u91cd\u7528\u5df2\u6709\u4ee3\u7801\u3002")
    };
    helpLabel_->setText(helps[currentType_]);
    inputStack_->setCurrentIndex(currentType_);
    onUrlChanged(currentInput());
}

void RepoPage::onBrowseLocal()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("\u9009\u62e9\u672c\u5730\u4ed3\u5e93\u76ee\u5f55"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        localEdit_->setText(QDir::toNativeSeparators(dir));
    }
}

void RepoPage::onBrowseCache()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("\u9009\u62e9\u672c\u5730\u7f13\u5b58\u76ee\u5f55"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        cacheEdit_->setText(QDir::toNativeSeparators(dir));
    }
}

void RepoPage::onUrlChanged(const QString& text)
{
    QString trimmed = text.trimmed();
    bool valid = !trimmed.isEmpty();

    const QString neutralColor = darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#666");
    const QString warnColor    = darkMode_ ? QStringLiteral("#e0aaff") : QStringLiteral("#e67e22");

    if (valid && currentType_ == SourceRemote) {
        bool looksValid =
            trimmed.startsWith(QLatin1String("https://"))
            || trimmed.startsWith(QLatin1String("http://"))
            || trimmed.startsWith(QLatin1String("git@"))
            || trimmed.startsWith(QLatin1String("ssh://"))
            || trimmed.startsWith(QLatin1String("file://"))
            || QDir(trimmed).exists();
        if (!looksValid) {
            hintLabel_->setText(
                QStringLiteral("\u63d0\u793a: \u8bf7\u8f93\u5165\u5b8c\u6574 Git URL\u3001SSH \u5730\u5740\u6216\u6709\u6548\u7684\u672c\u5730\u8def\u5f84"));
            hintLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(warnColor));
        } else {
            hintLabel_->setText(QStringLiteral(""));
            hintLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(neutralColor));
        }
    } else if (valid && (currentType_ == SourceLocal || currentType_ == SourceCache)) {
        if (!QDir(trimmed).exists()) {
            hintLabel_->setText(
                QStringLiteral("\u63d0\u793a: \u8be5\u8def\u5f84\u4e0d\u5b58\u5728\uff0c\u8bf7\u68c0\u67e5\u662f\u5426\u9009\u62e9\u6b63\u786e\u3002"));
            hintLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(warnColor));
        } else {
            hintLabel_->setText(QStringLiteral(""));
            hintLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(neutralColor));
        }
    } else {
        hintLabel_->setText(QStringLiteral(""));
        hintLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(neutralColor));
    }

    hintLabel_->setVisible(!hintLabel_->text().isEmpty());

    emit validityChanged(valid);
}

void RepoPage::onUrlReturnPressed()
{
    validateAndProceed();
}

void RepoPage::onRecentSelected(int index)
{
    if (index < 0) return;
    applyRecentIndex(index);
    const RecentEntry& e = recentEntries_[index];
    saveRecentRepo(e.location, e.cachePath);
    emit repoReady(e.location);
}

int RepoPage::sourceTypeForInput() const
{
    const QString url = repoUrl();
    return (url.startsWith(QLatin1String("http"))
            || url.startsWith(QLatin1String("git@"))
            || url.startsWith(QLatin1String("ssh")))
        ? SourceRemote : SourceLocal;
}

void RepoPage::applyRecentIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(recentEntries_.size())) return;

    const RecentEntry& e = recentEntries_[index];
    if (e.location.isEmpty()) return;

    currentType_ = e.type;
    if (e.type >= 0 && e.type < 3) {
        QPushButton* btn = typeBtns_[e.type];
        if (btn) {
            const QSignalBlocker blocker(btn);
            btn->setChecked(true);
        }
    }
    switch (e.type) {
    case SourceRemote:
        remoteEdit_->setText(e.location);
        break;
    case SourceCache:
        cacheEdit_->setText(e.location);
        break;
    case SourceLocal:
    default:
        localEdit_->setText(e.location);
        break;
    }
    onTypeChanged();
}

void RepoPage::validateAndProceed()
{
    if (!isValid()) return;

    QString url = repoUrl();
    saveRecentRepo(url);
    emit repoReady(url);
}

void RepoPage::applyTheme()
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    QString btnStyle;
    QString helpStyle;
    QString textStyle;
    if (darkMode_) {
        btnStyle = QStringLiteral(
            "QPushButton {"
            "  background: #2d2d30; color: #e8e8e8; border: 1px solid #55585e; border-radius: 8px;"
            "  padding: 3px 12px; font-size: 13px;"
            "}"
            "QPushButton:hover { border: 1px solid #0078d4; }"
            "QPushButton:checked {"
            "  background: #1f3b57; color: #ffffff; border: 2px solid #0078d4; font-weight: bold;"
            "}");
        helpStyle = QStringLiteral(
            "color: #c9c9cf; font-size: 12px; background: #252528; border: 1px solid #3f3f46;"
            "border-radius: 6px; padding: 4px 8px;");
        textStyle = QStringLiteral("color: #9da2aa; font-size: 12px;");
    } else {
        btnStyle = QStringLiteral(
            "QPushButton {"
            "  background: #ffffff; border: 1px solid #d0d0d0; border-radius: 8px;"
            "  padding: 3px 12px; font-size: 13px;"
            "}"
            "QPushButton:hover { border: 1px solid #0078d4; }"
            "QPushButton:checked {"
            "  background: #eaf3fd; border: 2px solid #0078d4; font-weight: bold;"
            "}");
        helpStyle = QStringLiteral(
            "color: #555; font-size: 12px; background: #f7f9fb; border: 1px solid #e3e6ea;"
            "border-radius: 6px; padding: 4px 8px;");
        textStyle = QStringLiteral("color: #666; font-size: 12px;");
    }

    for (auto* btn : typeBtns_) {
        btn->setStyleSheet(btnStyle);
    }
    helpLabel_->setStyleSheet(helpStyle);

    if (darkMode_) {
        recentCombo_->setStyleSheet(QStringLiteral(
            "QComboBox { background: #2d2d30; color: #e8e8e8; border: 1px solid #55585e;"
            "  border-radius: 4px; padding: 3px 6px; }"
            "QComboBox QAbstractItemView { background: #252528; color: #e8e8e8;"
            "  selection-background-color: #1f3b57; selection-color: #ffffff; }"));
    } else {
        recentCombo_->setStyleSheet(QString());
    }

    const QList<QLabel*> labels = findChildren<QLabel*>();
    for (auto* label : labels) {
        if (label->property("repoSubLabel").toBool()) {
            label->setStyleSheet(textStyle);
        }
    }
    if (hintLabel_->text().isEmpty()) {
        hintLabel_->setStyleSheet(textStyle);
    }
}

void RepoPage::loadRecentRepos(bool filterByType)
{
    recentEntries_.clear();

    auto stored = NeoWorkspace::HistoryStore::readRecentRepos();
    for (const auto& e : stored) {
        RecentEntry gui;
        gui.type = static_cast<int>(e.type);
        gui.location = QString::fromStdString(e.location);
        gui.cachePath = QString::fromStdString(e.cachePath);
        if (!gui.location.isEmpty()
            && (!filterByType || gui.type == currentType_)) {
            recentEntries_.push_back(gui);
        }
    }

    const QSignalBlocker blocker(recentCombo_);
    recentCombo_->clear();
    if (recentEntries_.empty()) {
        recentCombo_->addItem(QStringLiteral("\uff08\u6682\u65e0\u6700\u8fd1\u4ed3\u5e93\uff09"));
        recentCombo_->setCurrentIndex(0);
        return;
    }
    for (const auto& e : recentEntries_) {
        QString typeName = e.type == SourceRemote ? QStringLiteral("\u8fdc\u7a0b")
            : e.type == SourceCache ? QStringLiteral("\u7f13\u5b58")
            : QStringLiteral("\u672c\u5730");
        recentCombo_->addItem(typeName + QStringLiteral(" - ") + e.location);
    }
    recentCombo_->setCurrentIndex(0);
    applyRecentIndex(0);
}

void RepoPage::recordRecentCache(const QString& url, const QString& cachePath)
{
    if (url.trimmed().isEmpty() || cachePath.isEmpty()) return;
    saveRecentRepo(url, cachePath);
}

void RepoPage::commitToHistory()
{
    QString url = repoUrl();
    if (url.trimmed().isEmpty()) return;
    saveRecentRepo(url);
}

void RepoPage::saveRecentRepo(const QString& url, const QString& cachePath)
{
    QString qurl = url.trimmed();
    if (qurl.isEmpty()) return;

    NeoWorkspace::HistoryStore::saveRecentRepo(
        qurl.toStdString(),
        static_cast<NeoWorkspace::RepoType>(sourceTypeForInput()),
        cachePath.toStdString());

    loadRecentRepos(true);
}

} // namespace GUIWorker

#include "repo_page.moc"