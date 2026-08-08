#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QCheckBox>
#include <QListWidget>
#include <QLibrary>
#include <QJsonObject>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>

#include <QPointer>

#include <IPluginPointer.h>
#include <IPointerEditorExtension.h>

namespace GUIWorker {


struct ConvertedItem {
    QString sha;
    QString relPath;
    QString cacheAbs;
    QString pointerJson;
};

} // namespace GUIWorker

Q_DECLARE_METATYPE(GUIWorker::ConvertedItem)

namespace GUIWorker {

// 指针编辑器（P3）：resolvers 多解析器 + 下载方式 + 保存；转回原始文件；普通文件转指针
class PointerEditorPanel : public QWidget {
    Q_OBJECT

public:
    explicit PointerEditorPanel(QWidget* parent = nullptr);
    ~PointerEditorPanel() override;

    void setContext(const QString& repoDir, const QString& branch,
        const QString& branchConfigDir);
    void loadPointer(const QString& sha, const NeoCore::PointerFileData& data);
    void loadFileToConvert(const QString& relPath, const QString& absPath);
    void batchConvertJars(const QString& folderPath);

signals:
    void pointerSaved(const QString& sha);
    void branchConfigChanged(const QString& branch);
    void gitAddRequested(const QStringList& paths);
    void requestRefresh();
    void logMessage(const QString& line);
    void batchConvertFinished(const QVector<ConvertedItem>& items, int failed);

private slots:
    void onSavePointer();
    void onRestoreToFile();
    void onConvertCurrent();
    void onAddResolver();
    void onRemoveResolver();
    void onResolverTypeChanged();

public:
    // 撤销批量转换：把缓存中的 jar 移回分支目录 + 删 .pointer + 清 branch_config 登记
    void undoBatchConvert(const QVector<ConvertedItem>& items);
    // 重做批量转换：把分支目录中的 jar 移回缓存 + 重建 .pointer + 重登记 branch_config（用转换时保存的 pointerJson）
    void redoBatchConvert(const QVector<ConvertedItem>& items);
    // 导入撤销辅助：同 undoBatchConvert 但不清除已删除的 pointer 记录（供导入命令聚合）
    void removeConverted(const QVector<ConvertedItem>& items);
    // 从 pointer-cache 回写文件并清理指针登记（零信任：先验 SHA-256）
    bool restorePointerFromCache(const QString& sha,
        const NeoCore::PointerFileData& data);

private:
    struct ResolverEditor {
        NeoCore::IPointerEditorExtension* extension = nullptr;
        QWidget* widget = nullptr;
    };

    void loadExtensions();
    void unloadExtensions();
    void clearResolverEditors();
    NeoCore::PointerFileData currentData() const;
    static bool updateBranchConfig(const std::string& bcDir,
        const std::string& branch, const std::string& relPath,
        const std::string& sha, const NeoCore::PointerInfo& info,
        bool removePointer);
    std::string branchConfigPath() const;
    void doBatchConvert(const QString& folderPath);
    void appendLog(const QString& line);

    QString repoDir_;
    QString branch_;
    QString branchConfigDir_;
    QString currentSha_;
    QString currentRelPath_;
    QString currentAbsPath_;
    bool convertMode_ = false;

    std::unique_ptr<std::thread> batchThread_;

    std::vector<NeoCore::IPointerEditorExtension*> loadedExtensions_;
    std::map<QString, NeoCore::IPointerEditorExtension*> extRegistry_;
    std::vector<QLibrary*> libs_;
    std::vector<ResolverEditor> resolverEditors_;

    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QLineEdit* shaEdit_;
    QTextEdit* namesEdit_;
    QComboBox* resolverTypeCombo_;
    QStackedWidget* editorStack_;
    QPushButton* addResolverBtn_;
    QPushButton* removeResolverBtn_;
    QCheckBox* curlCheck_;
    QCheckBox* psCheck_;
    QCheckBox* qtCheck_;
    QPushButton* saveButton_;
    QPushButton* restoreButton_;
    QPushButton* convertButton_;
    QLabel* convertHint_;
    QLabel* batchInfoLabel_;

    static const int MaxResolvers = 6;
};

} // namespace GUIWorker
