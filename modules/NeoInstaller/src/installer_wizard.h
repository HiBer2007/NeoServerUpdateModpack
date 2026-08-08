#pragma once

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QProgressBar>
#include <QPushButton>
#include <QProcess>
#include <QRadioButton>
#include <string>

namespace NeoInstaller {

class WelcomePage : public QWizardPage {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget* parent = nullptr);
};

class LicensePage : public QWizardPage {
    Q_OBJECT
public:
    explicit LicensePage(QWidget* parent = nullptr);
    bool isComplete() const override;

private:
    QCheckBox* acceptCheck_;
    QTextEdit* licenseText_;
};

class InstallPathPage : public QWizardPage {
    Q_OBJECT
public:
    explicit InstallPathPage(QWidget* parent = nullptr);
    bool isComplete() const override;
    bool validatePage() override;

private slots:
    void onBrowse();

private:
    QLineEdit* pathEdit_;
    QPushButton* browseBtn_;
};

class ComponentPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ComponentPage(QWidget* parent = nullptr);
    void initializePage() override;
    int nextId() const override;
    bool isEditorSelected() const;

private:
    QCheckBox* editorCheck_;
};

class GitChoicePage : public QWizardPage {
    Q_OBJECT
public:
    explicit GitChoicePage(QWidget* parent = nullptr);
    void initializePage() override;
    int nextId() const override;

    bool useSystemGit() const;
    bool needsGitLicense() const;

private:
    QLabel* statusLabel_;
    QRadioButton* systemGitRadio_;
    QRadioButton* bundledGitRadio_;
    bool systemGitAvailable_;
};

class GitLicensePage : public QWizardPage {
    Q_OBJECT
public:
    explicit GitLicensePage(QWidget* parent = nullptr);
    bool isComplete() const override;

private:
    QCheckBox* acceptCheck_;
};

class ProgressPage : public QWizardPage {
    Q_OBJECT
    friend class FinishPage;
public:
    explicit ProgressPage(QWidget* parent = nullptr);
    void initializePage() override;
    bool isComplete() const override;

private slots:
    void performInstallation();
    void updateProgress(int percent, const QString& step, const QString& detail);

private:
    QProgressBar* progressBar_;
    QLabel* stepLabel_;
    QTextEdit* logView_;
    QLabel* statusIcon_;
    bool installComplete_;
    bool installSuccess_;
    QProcess* dlProc_ = nullptr;

    QString fieldInstallPath() const;
    bool createDesktopShortcut(const QString& targetPath, const QString& installPath);
    bool createStartMenuShortcut(const QString& targetPath, const QString& installPath);
    void writeInstallConfig(const QString& installPath);
};

class FinishPage : public QWizardPage {
    Q_OBJECT
public:
    explicit FinishPage(QWidget* parent = nullptr);
    void initializePage() override;

private:
    QLabel* finishLabel_;
    QCheckBox* launchCheck_;
};

class InstallerWizard : public QWizard {
    Q_OBJECT
public:
    explicit InstallerWizard(QWidget* parent = nullptr);
    ~InstallerWizard();

    void setInstallEditor(bool on) { installEditor_ = on; }
    bool installEditor() const { return installEditor_; }
    QString gitVariant() const { return installEditor_ ? "portablegit" : "mingit"; }

    enum PageId {
        PAGE_WELCOME = 0,
        PAGE_LICENSE,
        PAGE_INSTALL_PATH,
        PAGE_COMPONENTS,
        PAGE_GIT_CHOICE,
        PAGE_GIT_LICENSE,
        PAGE_PROGRESS,
        PAGE_FINISH
    };

private:
    bool installEditor_ = false;
};

} // namespace NeoInstaller
