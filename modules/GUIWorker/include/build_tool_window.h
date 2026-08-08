#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>

namespace GUIWorker {

class BuildToolWindow : public QWidget {
    Q_OBJECT

public:
    explicit BuildToolWindow(QWidget* parent = nullptr);

    void appendOutput(const QString& text);
    void clearOutput();

signals:
    void commandEntered(const QString& command);

private slots:
    void onExecute();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void executeCommand(const QString& cmd);
    void appendLog(const QString& prefix, const QString& text);
    void showHelp();
    void navigateHistory(int direction);

    QTextEdit* outputEdit_;
    QLineEdit* commandEdit_;
    QPushButton* executeBtn_;

    QStringList commandHistory_;
    int historyIndex_;
    QString currentInput_;
};

} // namespace GUIWorker
