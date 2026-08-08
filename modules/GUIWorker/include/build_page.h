#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QStringList>
#include <map>
#include <thread>
#include <cancel_token.h>
#include <IBuildProgress.h>

namespace GUIWorker {

class BuildPage : public QWidget {
    Q_OBJECT

public:
    explicit BuildPage(QWidget* parent = nullptr);

    // 真实构建入口：后台线程执行导出插件 build_modpack，进度/日志经 IBuildProgress 回传
    void startBuild(QString repoDir, QString gitBranch, QString modpackBranch,
        QString exportFormat, QString outputPath);
    void cancelBuild();

    QString outputDir() const { return buildOutputDir_; }

    const QStringList& warnings() const { return buildWarnings_; }

signals:
    void buildFinished(bool success, QString message, QStringList warnings);
    void progressUpdated(QString stage, int percent, QString message);
    void subProgressUpdated(QString label, int done, int total);
    void logLine(QString line);
    void subBarUpdate(int handle, QString label, int percent, QString info);

public slots:
    void appendLog(const QString& msg);

private slots:
    void onCancelClicked();
    void onToggleDetails();
    void onBuildDone(bool success, const QString& message, const QStringList& warnings);
    void onProgressUpdated(QString stage, int percent, QString message);
    void onSubBarUpdate(int handle, QString label, int percent, QString info);

private:
    class GuiBuildProgress : public NeoCore::IBuildProgress {
    public:
        GuiBuildProgress(BuildPage* page, NeoCore::CancelToken* token)
            : page_(page), token_(token) {}

        void set_main_stage(const std::string& stage) override;
        void set_main_progress(int percent) override;
        void set_main_message(const std::string& message) override;
        int add_sub_bar(const std::string& label) override;
        void remove_sub_bar(int handle) override;
        void set_sub_progress(int handle, int percent) override;
        void set_sub_info(int handle, const std::string& info) override;
        void log(const std::string& line) override;
        bool is_cancelled() const override;

    private:
        BuildPage* page_;
        NeoCore::CancelToken* token_;
        std::string stage_;
        std::string message_;
        int percent_ = 0;
        int nextHandle_ = 1;
        std::map<int, std::string> labels_;
    };

    void runBuildWorker();
    void finishBuildUi(bool success, const QString& message, const QStringList& warnings);

    QProgressBar* mainBar_;
    QLabel* mainLabel_;
    QProgressBar* subBar_;
    QLabel* subLabel_;
    QPushButton* cancelBtn_;
    QPushButton* detailsBtn_;
    QTextEdit* logOutput_;
    QWidget* logContainer_;

    NeoCore::CancelToken cancelToken_;
    std::thread workerThread_;

    QString repoDir_;
    QString gitBranch_;
    QString modpackBranch_;
    QString exportFormat_;
    QString outputPath_;
    QString buildOutputDir_;
    QStringList buildWarnings_;

    bool buildInProgress_ = false;
    bool detailsVisible_ = false;
};

} // namespace GUIWorker
