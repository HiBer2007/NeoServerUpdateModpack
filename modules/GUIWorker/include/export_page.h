#pragma once

#include <animated_progress.h>

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QTextEdit>
#include <QLabel>

#include <string>
#include <vector>

namespace GUIWorker {
using HiBerGUI::AnimatedProgress;
}

namespace NeoCore {
struct ExportMetadata;
}

namespace GUIWorker {

class ExportPage : public QWidget {
    Q_OBJECT

public:
    explicit ExportPage(QWidget* parent = nullptr);

    void setBuildDir(QString dir);
    void setOutputPath(const QString& path);
    void setFormat(const QString& formatId);
    void loadFormats();

signals:
    void exportStarted(QString outputPath, QString format);

private slots:
    void onBrowseOutput();
    void onExport();
    void onFormatChanged(int index);

private:
    QLineEdit* outputPathEdit_;
    QComboBox* formatCombo_;
    QPushButton* browseBtn_;
    QPushButton* exportBtn_;
    QLabel* statusLabel_;
    QLabel* formatDescLabel_;
    QLineEdit* nameEdit_;
    QLineEdit* versionEdit_;
    QLineEdit* authorEdit_;
    QTextEdit* descEdit_;
    AnimatedProgress* progress_;

    QString buildDir_;
    std::vector<std::string> availableFormats_;
    std::vector<std::string> formatExtensions_;
    std::vector<std::string> formatDescriptions_;

    NeoCore::ExportMetadata gatherMetadata() const;
    void performExport();
};

} // namespace GUIWorker
