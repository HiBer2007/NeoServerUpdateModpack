#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <IPluginPointer.h>
#include <string>
#include <vector>

namespace GUIWorker {

class PointerEditor : public QWidget {
    Q_OBJECT

public:
    explicit PointerEditor(QWidget* parent = nullptr);

    void loadPointer(const NeoCore::PointerInfo& ptr);
    NeoCore::PointerInfo pointerInfo() const;
    void clear();

signals:
    void pointerModified();

private slots:
    void onAddRow();
    void onRemoveRow();
    void onResolverChanged(int index);
    void onCellChanged(int row, int column);

private:
    QLineEdit* sha256Edit_;
    QComboBox* resolverCombo_;
    QTableWidget* metadataTable_;
    QPushButton* addRowBtn_;
    QPushButton* removeRowBtn_;

    static const QStringList RESOLVER_TYPES;

    void ensureEmptyRow();
    QString currentResolver() const;
};

} // namespace GUIWorker

