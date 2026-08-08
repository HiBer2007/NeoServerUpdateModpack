#pragma once

#include <QWidget>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QLineEdit>

#include <string>
#include <vector>
#include <memory>

namespace NeoCore {
class PluginLoader;
class IConfigParser;
}

namespace GUIWorker {

// 配置文件同步编辑器（P2）：full/partial/ignore + tracked_keys/tracked_lines + merge 预览
class ConfigFileEditor : public QWidget {
    Q_OBJECT

public:
    explicit ConfigFileEditor(QWidget* parent = nullptr);
    ~ConfigFileEditor() override;

    void load(const QString& relativePath, const QString& absRepoPath,
        const QString& repoDir, const QString& branch,
        const QString& effectiveMode,
        const std::vector<std::string>& effectiveKeys,
        const std::vector<int>& effectiveLines,
        bool branchOverrides);

public slots:
    void setScopeTop();

signals:
    void saveRequested(const QString& relativePath, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void contentModified();

private:
    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QLabel* parserLabel_;
    QRadioButton* fullRb_;
    QRadioButton* partialRb_;
    QRadioButton* ignoreRb_;
    QListWidget* keysList_;
    QLineEdit* linesEdit_;
    QRadioButton* topRb_;
    QRadioButton* branchRb_;
    QPushButton* saveButton_;
    QPushButton* previewButton_;

    QString relativePath_;
    QString absRepoPath_;
    QString repoDir_;
    QString branch_;

    std::unique_ptr<NeoCore::PluginLoader> loader_;
    NeoCore::IConfigParser* parser_ = nullptr;

    void reloadKeys();
    void updateEnabled();
    void doPreview();
    void applyStyle();
};

} // namespace GUIWorker
