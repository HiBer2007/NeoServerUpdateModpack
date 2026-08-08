#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace HiBerGUI {

class FileContentEditor : public QWidget {
    Q_OBJECT

public:
    explicit FileContentEditor(QWidget* parent = nullptr);

    void loadContent(const QString& relPath, const QString& absPath,
        bool inherited, const QString& sourceAbs);

signals:
    void contentSaveRequested(const QString& relPath, const QString& content,
        bool inherited);

private:
    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QPlainTextEdit* view_;
    QPushButton* saveBtn_;

    QString relPath_;
    bool inherited_ = false;
};

} // namespace HiBerGUI
