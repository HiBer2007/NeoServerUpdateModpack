#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QListWidget>
#include <QScrollArea>
#include <QGroupBox>
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
#include <cancel_token.h>
#include "batch_convert_card.h"

namespace HiBerGUI {
class ProgressCard;
}

class QResizeEvent;
class QVBoxLayout;
class QFrame;

namespace GUIWorker {
class EditorExtensionRegistry;

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
    void setExtensionRegistry(EditorExtensionRegistry* reg);
    void loadPointer(const QString& sha, const NeoCore::PointerFileData& data);
    void loadFileToConvert(const QString& relPath, const QString& absPath);
    void batchConvertJars(const QString& folderPath);
    void batchConvertJarsList(const QStringList& relPaths);

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

protected:
    void resizeEvent(QResizeEvent* event) override;

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

    void clearResolverEditors();
    void populateResolverCombo();
    // 添加一张解析器卡片 (类型标题 + 编辑器内容 + 删除按钮), 返回编辑器控件
    QWidget* addResolverCard(const QString& type);
    // 卡片增删后强制重排重绘 (QScrollArea 动态内容不会自动失效视口)
    void refreshResolverArea();
    // 解析器板块高度上限 = 窗口剩余 (模式切换/控件显隐后主动重算)
    void updateResolverHeight();
    NeoCore::PointerFileData currentData() const;
    static bool updateBranchConfig(const std::string& bcDir,
        const std::string& branch, const std::string& relPath,
        const std::string& sha, const NeoCore::PointerInfo& info,
        bool removePointer);
    std::string branchConfigPath() const;
    void startBatchConvert(const QStringList& absPaths);
    void appendLog(const QString& line);

    QString repoDir_;
    QString branch_;
    QString branchConfigDir_;
    QString currentSha_;
    QString currentRelPath_;
    QString currentAbsPath_;
    bool convertMode_ = false;

    std::unique_ptr<std::thread> batchThread_;
    std::unique_ptr<NeoCore::CancelToken> batchCancelToken_;
    BatchConvertCard* batchCard_ = nullptr;

    std::map<QString, NeoCore::IPointerEditorExtension*> extRegistry_;
    EditorExtensionRegistry* extReg_ = nullptr;
    std::vector<ResolverEditor> resolverEditors_;

    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QLineEdit* shaEdit_;
    QTextEdit* namesEdit_;
    QComboBox* resolverTypeCombo_;
    QPushButton* addResolverBtn_;
    QPushButton* saveButton_;
    QPushButton* restoreButton_;
    QPushButton* convertButton_;
    QLabel* convertHint_;
    QLabel* batchInfoLabel_;

    // 解析器板块 (动态高度, 上限为窗口剩余) 与下载方式 (有序列表)
    QGroupBox* idGroup_;
    QGroupBox* namesGroup_;
    QGroupBox* dlGroup_;
    QScrollArea* scrollArea_;
    // 解析器卡片列表: 每张卡片 = 类型标题 + 编辑器内容 + 删除按钮
    QFrame* resolverListHost_ = nullptr;
    QVBoxLayout* resolverListLayout_ = nullptr;
    QListWidget* dlList_;
    QPushButton* dlUpBtn_;
    QPushButton* dlDownBtn_;

    static const int MaxResolvers = 6;
};

} // namespace GUIWorker
