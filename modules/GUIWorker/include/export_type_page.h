#pragma once

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <string>
#include <vector>

namespace GUIWorker {

class ExportTypePage : public QWidget {
    Q_OBJECT

public:
    explicit ExportTypePage(QWidget* parent = nullptr);

    QString selectedFormat() const;
    QString selectedFormatName() const;
    bool    hasSelection() const;
    void    selectFormat(const QString& id);

signals:
    void formatSelected(QString format);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    struct ExportFormat {
        std::string id;
        std::string name;
        std::string extension;
        std::string description;
    };

    QFrame* createCard(const ExportFormat& fmt, int index);
    void    updateSelection();
    QString cardStyle(bool selected) const;
    void    scanExporters();

    std::vector<ExportFormat> formats_;
    std::vector<QFrame*>      cards_;
    int                       selectedIndex_ = -1;
    bool                      darkMode_ = false;
};

} // namespace GUIWorker
