#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QLabel>

namespace GUIWorker {

class ExportDirPage : public QWidget {
    Q_OBJECT

public:
    explicit ExportDirPage(QWidget* parent = nullptr);

    void    setContext(const QString& modpackBranch, const QString& formatId,
                       const QString& extension);
    QString outputPath() const;
    bool    isValid() const;
    void    setPath(const QString& path);

signals:
    void dirSelected(QString path);

private slots:
    void onBrowse();
    void onPathChanged(const QString& text);

private:
    QLineEdit*   dirEdit_       = nullptr;
    QPushButton* browseBtn_     = nullptr;
    QLabel*      titleLabel_    = nullptr;
    QLabel*      subLabel_      = nullptr;

    QString modpackBranch_;
    QString formatId_;
    QString extension_;
    bool    directoryMode_ = false;
};

} // namespace GUIWorker