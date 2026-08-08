#pragma once

#include <toast_notification.h>

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QScopedPointer>
#include <QProgressBar>
#include <QLabel>
#include <QFrame>
#include <QMap>

namespace GUIWorker {

class RepoPage;
class BranchPage;
class ModpackPage;
class ExportTypePage;
class ExportDirPage;
class ExtraInfoPage;
class BuildChecklistPage;
class BuildPage;
class DonePage;
using HiBerGUI::ToastNotification;

// Flow (CLI flow gui) configuration. Page names:
// repo|branch|modpack|export-type|export-dir|extra-info|checklist|build|done
struct FlowConfig {
    QString startPage;
    QString endPage;
    bool collectOnly = false;
    QMap<QString, QString> prefill;
};

class WizardWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit WizardWindow(QWidget* parent = nullptr);
    ~WizardWindow();

    void setFlowMode(const FlowConfig& cfg);
    static int pageNameToIndex(const QString& name);
    void flowTriggerNext();
    bool flowDone() const { return flowDone_; }

signals:
    void flowDataReady(const QString& json);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNext();
    void onPrev();
    void onCancel();
    void onRepoReady(QString url);
    void onBranchSelected(QString branch);
    void onModpackSelected(QString modpack);
    void onExportTypeSelected(QString format);
    void onBuildFinished(bool success, QString message, QStringList warnings);
    void onCloneFinished(int exitCode, QProcess::ExitStatus status);
private:
    QStackedWidget* stack_;
    QPushButton* btnPrev_;
    QPushButton* btnNext_;
    QPushButton* btnCancel_;

    RepoPage* repoPage_;
    BranchPage* branchPage_;
    ModpackPage* modpackPage_;
    ExportTypePage* exportTypePage_;
    ExportDirPage* exportDirPage_;
    ExtraInfoPage* extraInfoPage_;
    BuildChecklistPage* buildChecklistPage_;
    BuildPage* buildPage_;
    DonePage* donePage_;
    ToastNotification* toast_ = nullptr;

    QWidget* progressOverlay_ = nullptr;
    QFrame* progressCard_ = nullptr;
    QLabel* progressTitleLabel_ = nullptr;
    QLabel* progressStatusLabel_ = nullptr;
    QProgressBar* progressMainBar_ = nullptr;
    QLabel* progressPercentLabel_ = nullptr;
    QProgressBar* progressSubBar_ = nullptr;
    QLabel* progressSubLabel_ = nullptr;

    QProcess* cloneProcess_ = nullptr;
    QProcess* syncProcess_  = nullptr;

    int currentPage_;
    QString repoUrl_;
    QString repoLocalPath_;
    QString gitBranch_;
    QString modpackBranch_;
    QString exportFormat_;
    QString exportOutputPath_;
    QString buildDir_;
    bool remoteSource_ = false;
    bool positioned_ = false;
    bool lastBuildFailed_ = false;

    static const int PAGE_REPO = 0;
    static const int PAGE_BRANCH = 1;
    static const int PAGE_MODPACK = 2;
    static const int PAGE_EXPORT_TYPE = 3;
    static const int PAGE_EXPORT_DIR = 4;
    static const int PAGE_EXTRA_INFO = 5;
    static const int PAGE_BUILD_CHECKLIST = 6;
    static const int PAGE_BUILD = 7;
    static const int PAGE_DONE = 8;

    void navigateTo(int page);
    void updateButtons();
    void handlePageActivation(int page);
    void populateChecklist();
    void showError(const QString& title, const QString& detail = QString());
    void adjustWindowSize(bool animate);
    void fadeTransition();
    void startClone(const QString& url);
    void startSyncCache(const QString& dir);
    void cleanupTempRepo();
    void showProgressDialog(const QString& text, bool indeterminate, bool withSub);
    void setProgressTitle(const QString& title);
    void hideProgressDialog();
    void parseGitProgress(const QString& line, bool isFetch);
    void finishRepoLoading();
    void buildProgressCard();
    bool isRemoteUrl(const QString& url) const;
    void openHelp();

    // flow (CLI flow gui) helpers
    static QString indexToPageName(int index);
    void flowInit();
    void flowMaybeAdvanceFrom(int page);
    void flowAdvanceNext();
    QString flowCollectJson() const;
    void flowFinish();

    FlowConfig flowCfg_;
    bool flowActive_ = false;
    bool flowDone_ = false;
    int flowEndIndex_ = -1;
    int flowStartIndex_ = -1;
};

} // namespace GUIWorker