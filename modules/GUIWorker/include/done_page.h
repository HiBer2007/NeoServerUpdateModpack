#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

namespace GUIWorker {

class DonePage : public QWidget {
    Q_OBJECT

public:
    explicit DonePage(QWidget* parent = nullptr);

    void showSuccess(const QString& outputDir, const QStringList& warnings = QStringList());
    void showFailure(const QString& reason, const QString& suggestion);

signals:
    void openOutputDirRequested(const QString& dir);
    void finishRequested();

private slots:
    void onShowWarningDetails();

private:
    QLabel* iconLabel_;
    QLabel* titleLabel_;
    QLabel* messageLabel_;
    QLabel* suggestionLabel_;
    QLabel* warningLabel_;
    QPushButton* warnDetailsBtn_;
    QPushButton* helpBtn_;
    QPushButton* openDirBtn_;
    QPushButton* exitBtn_;
    QString outputDir_;
    QStringList warnings_;
    bool darkMode_ = false;
};

} // namespace GUIWorker
