#pragma once

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QLabel;

namespace GUIWorker {

class SshKeyDialog : public QDialog {
    Q_OBJECT

public:
    explicit SshKeyDialog(QWidget* parent = nullptr);

    void setKeyPath(const QString& path);
    QString keyPath() const;
    QString host() const;

private slots:
    void onBrowse();
    void onGenerate();
    void onCopyPublicKey();
    void onTest();
    void onConfigure();
    void onSave();

private:
    void refreshKey();
    void setStatus(const QString& text, bool ok);

    QLineEdit* hostEdit_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QLineEdit* commentEdit_ = nullptr;
    QPlainTextEdit* publicKeyEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

} // namespace GUIWorker
