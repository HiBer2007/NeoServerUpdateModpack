#pragma once

#include <QWidget>
#include <QLabel>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QScrollArea>
#include <nlohmann/json.hpp>

namespace GUIWorker {

class ExtraInfoPage : public QWidget {
    Q_OBJECT

public:
    explicit ExtraInfoPage(QWidget* parent = nullptr);

    bool loadFormat(const QString& formatId);
    bool hasFields() const;
    bool validate() const;
    QMap<QString, QString> values() const;
    bool setValue(const QString& key, const QString& value);
    QStringList missingRequired() const;
    QString formatName() const;
    void markMissing(const QStringList& missing);

    int contentHeight(int widthHint) const;

signals:
    void validityChanged(bool valid);

private:
    struct FieldDef {
        QString key;
        QString label;
        QString group;
        QString type;
        QString placeholder;
        bool required = false;
    };

    QWidget* buildFieldWidget(const FieldDef& f);
    void applyTheme();
    void updateLabelColors();
    QString lineEditStyle(bool error) const;
    QString textEditStyle(bool error) const;
    QString labelStyle(bool error) const;

    QString formatId_;
    QString formatName_;
    QList<FieldDef> fields_;
    QMap<QString, QWidget*> inputWidgets_;
    QMap<QString, QLabel*> fieldLabels_;
    QList<QPair<QString, QWidget*>> groupWidgets_;

    QScrollArea* scroll_;
    QWidget* content_;
    QLabel* statusLabel_;
    bool darkMode_ = false;
};

} // namespace GUIWorker
