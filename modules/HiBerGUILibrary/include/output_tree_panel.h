#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QEvent>

#include <nlohmann/json.hpp>

namespace HiBerGUI {

class OutputTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutputTreePanel(QWidget* parent = nullptr);

    void loadEntries(const nlohmann::json& entries);
    void setStatusText(const QString& text);
    void setFormat(const QString& format);
    QString format() const;
    QTreeWidget* tree() const { return tree_; }

    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void formatChanged(const QString& format);
    void refreshRequested();
    void filesDropped(const QStringList& filePaths, const QString& targetRel);

private:
    QTreeWidget* tree_;
    QLabel* statusLabel_;
    QComboBox* formatCombo_;
    QPushButton* refreshButton_;

    QString relPathAt(const QPoint& pos) const;
    void applyStyle();
};

} // namespace HiBerGUI
