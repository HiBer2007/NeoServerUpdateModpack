#pragma once

#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace NeoCore {
class PluginLoader;
}

namespace GUIWorker {

// serverconfig 规则编辑（P4）：
// 仓库 branches/<branch>/[save]/serverconfig/（[save] 字面目录名）
//   .rule/globle.json -> { default_mode, version, description }
//   .rule/list.json   -> { files: { <rel>: {mode, tracked_keys} } } (兼容旧字符串格式)
//   .rule/<其他文件>    -> 规则文件组（只读展示）
// 模式与 config 同步逻辑统一: full/force/partial/ignore, partial 支持 tracked_keys。
class ServerConfigRulesEditor : public QWidget {
    Q_OBJECT

public:
    explicit ServerConfigRulesEditor(QWidget* parent = nullptr);
    ~ServerConfigRulesEditor() override;

    void setContext(const QString& repoDir, const QString& branch);
    void load();

signals:
    void gitAddRequested(const QStringList& paths);
    void logMessage(const QString& line);

private slots:
    void onAddSourceFile();
    void onRemoveSourceFile();
    void onSave();
    void onModeChanged(int row, int column);

private:
    QString scDir() const;
    QString ruleDir() const;
    void loadGloble();
    void loadList();
    void loadSourceFiles();
    void loadReserved();
    void buildModeTable();
    void updateParserHint(int row);
    void appendLog(const QString& line);

    QString repoDir_;
    QString branch_;
    std::unique_ptr<NeoCore::PluginLoader> loader_;

    QString globleVersion_;
    QString globleDescription_;
    std::map<QString, QString> listedModes_;
    std::map<QString, QString> listedKeys_;   // rel -> tracked_keys 逗号分隔
    QStringList sourceFiles_;
    QStringList reservedFiles_;

    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QListWidget* sourceList_;
    QPushButton* addSourceBtn_;
    QPushButton* removeSourceBtn_;
    QComboBox* defaultModeCombo_;
    QComboBox* folderModeCombo_;
    QTableWidget* modeTable_;
    QTextEdit* reservedView_;
    QPushButton* saveButton_;
    QLabel* hintLabel_;
};

} // namespace GUIWorker
