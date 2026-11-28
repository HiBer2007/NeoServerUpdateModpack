#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <string>
#include <vector>

namespace GUIWorker {

class RepoPage : public QWidget {
    Q_OBJECT

public:
    explicit RepoPage(QWidget* parent = nullptr);

    QString repoUrl() const;
    bool isValid() const;
    void setUrl(const QString& url);
    int sourceType() const { return currentType_; }
    QString repoPreference() const;
    void recordRecentCache(const QString& url, const QString& cachePath);
    void commitToHistory();

    enum SourceType { SourceRemote = 0, SourceLocal = 1, SourceCache = 2 };

signals:
    void repoReady(QString url);
    void validityChanged(bool valid);

private slots:
    void onTypeChanged();
    void onBrowseLocal();
    void onBrowseCache();
    void onUrlChanged(const QString& text);
    void onUrlReturnPressed();
    void onRecentSelected(int index);
    void onSshKey();

private:
    struct RecentEntry {
        int     type = SourceRemote;
        QString location;
        QString cachePath;
    };

    QLineEdit*   remoteEdit_  = nullptr;
    QLineEdit*   localEdit_   = nullptr;
    QLineEdit*   cacheEdit_   = nullptr;
    QPushButton* localBrowse_ = nullptr;
    QPushButton* cacheBrowse_ = nullptr;
    QPushButton* sshKeyBtn_ = nullptr;
    QStackedWidget* inputStack_ = nullptr;
    QComboBox* recentCombo_ = nullptr;
    QComboBox* preferenceCombo_ = nullptr;
    QLabel*      hintLabel_  = nullptr;
    QLabel*      helpLabel_  = nullptr;
    QList<QPushButton*> typeBtns_;
    std::vector<RecentEntry> recentEntries_;

    int currentType_ = SourceRemote;
    bool darkMode_ = false;
    QString currentInput() const;

    void applyTheme();
    void loadRecentRepos(bool filterByType = false);
    void saveRecentRepo(const QString& url, const QString& cachePath = QString());
    void applyRecentIndex(int index);
    void validateAndProceed();
    int sourceTypeForInput() const;
};

} // namespace GUIWorker