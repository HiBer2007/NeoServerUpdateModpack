#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QStackedWidget>
#include <QLibrary>
#include <QJsonObject>
#include <IPluginPointer.h>
#include <IPointerEditorExtension.h>
#include <string>
#include <vector>
#include <map>

namespace GUIWorker {

class PointerManager : public QDialog {
public:
    explicit PointerManager(QWidget* parent = nullptr,
        const std::string& branchConfigDir = "");
    ~PointerManager();

    void setBranchConfigDir(const std::string& dir);
    void onSelectPointer();
    void onNewPointer();
    void onDeletePointer();
    void onSavePointer();
    void onAddResolver();
    void onRemoveResolver();
    void onBatchConvertJars();
    void onResolverTypeChanged(int index);
    void onCurrentChanged();

private:
    struct ResolverEditor {
        NeoCore::IPointerEditorExtension* extension = nullptr;
        QWidget* widget = nullptr;
    };

    void buildUI();
    void refreshList();
    void loadPointerFile(const std::string& sha256);
    void clearDetail();
    bool saveCurrentPointer();
    std::vector<std::string> scanPointerFiles() const;
    NeoCore::PointerFileData currentData() const;
    ResolverEditor createEditorForType(const QString& type, QWidget* parent);
    void loadExtensions();
    void unloadExtensions();

    std::string ptrDir_;
    std::vector<NeoCore::IPointerEditorExtension*> loadedExtensions_;
    std::map<QString, NeoCore::IPointerEditorExtension*> extRegistry_;
    std::vector<QLibrary*> libs_;

    QListWidget* ptrList_;
    QLineEdit* sha256Edit_;
    QTextEdit* namesEdit_;

    QGroupBox* resolversGroup_;
    QComboBox* resolverTypeCombo_;
    QStackedWidget* editorStack_;
    std::vector<ResolverEditor> resolverEditors_;

    QPushButton* addResolverBtn_;
    QPushButton* removeResolverBtn_;

    QCheckBox* curlCheck_;
    QCheckBox* psCheck_;
    QCheckBox* qtCheck_;
    QPushButton* batchBtn_;
    QPushButton* saveBtn_;
    QPushButton* deleteBtn_;
    QPushButton* newBtn_;

    bool modified_ = false;
    std::map<std::string, NeoCore::PointerFileData> cache_;
};

} // namespace GUIWorker
