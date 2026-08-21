#include "editor_modrinth.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QApplication>

#include <nlohmann/json.hpp>
#include <IPointerEditorExtension.h>
#include <logger.h>

ModrinthEditor::ModrinthEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox("Modrinth 元数据", this);
    auto* form = new QFormLayout(group);

    projectIdEdit_ = new QLineEdit(group);
    projectIdEdit_->setPlaceholderText("AANobbMI (Project ID / Slug)");
    form->addRow("Project ID:", projectIdEdit_);

    versionIdEdit_ = new QLineEdit(group);
    versionIdEdit_->setPlaceholderText("aOPSMNeo (Version ID)");
    form->addRow("Version ID:", versionIdEdit_);

    auto* fetchRow = new QHBoxLayout();
    fetchBtn_ = new QPushButton("获取模组信息", group);
    statusLabel_ = new QLabel("", group);
    fetchRow->addWidget(fetchBtn_);
    fetchRow->addWidget(statusLabel_);
    fetchRow->addStretch();
    form->addRow("", fetchRow);

    modNameEdit_ = new QLineEdit(group);
    modNameEdit_->setReadOnly(true);
    modNameEdit_->setPlaceholderText("(自动获取)");
    form->addRow("名称:", modNameEdit_);

    layout->addWidget(group);
    layout->addStretch();

    QObject::connect(fetchBtn_, &QPushButton::clicked, [this]() {
        QString pid = projectIdEdit_->text().trimmed();
        QString vid = versionIdEdit_->text().trimmed();
        if (pid.isEmpty()) { statusLabel_->setText("请输入 Project ID"); return; }
        fetchBtn_->setEnabled(false);
        statusLabel_->setText("查询中...");
        QApplication::processEvents();

        QUrl url;
        if (!vid.isEmpty())
            url = QUrl(QString("https://api.modrinth.com/v2/project/%1/version/%2").arg(pid, vid));
        else
            url = QUrl(QString("https://api.modrinth.com/v2/project/%1").arg(pid));

        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0");
        req.setTransferTimeout(15000);
        QNetworkAccessManager mgr;
        QNetworkReply* reply = mgr.get(req);
        QEventLoop loop;
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            reply->deleteLater();
            try {
                auto j = nlohmann::json::parse(data.toStdString());
                std::string name = j.value("title", j.value("name", ""));
                if (name.empty()) name = j.value("version_number", "");
                if (!name.empty()) {
                    modNameEdit_->setText(QString::fromStdString(name));
                    statusLabel_->setText("已获取");
                    statusLabel_->setStyleSheet("color: green;");
                }
            } catch (...) {
                statusLabel_->setText("解析失败");
                statusLabel_->setStyleSheet("color: red;");
            }
        } else {
            statusLabel_->setText("获取失败");
            statusLabel_->setStyleSheet("color: red;");
            reply->deleteLater();
        }
        fetchBtn_->setEnabled(true);
    });
}

void ModrinthEditor::loadMetadata(const QJsonObject& metadata)
{
    projectIdEdit_->setText(metadata.value("project_id").toString());
    versionIdEdit_->setText(metadata.value("version_id").toString());
    modNameEdit_->setText(metadata.value("name").toString());
}

QJsonObject ModrinthEditor::saveMetadata() const
{
    QJsonObject meta;
    meta["project_id"] = projectIdEdit_->text().trimmed();
    meta["version_id"] = versionIdEdit_->text().trimmed();
    if (!modNameEdit_->text().trimmed().isEmpty())
        meta["name"] = modNameEdit_->text().trimmed();
    return meta;
}

class ModrinthEditorExtension : public NeoCore::IPointerEditorExtension {
public:
    std::string resolverType() const override { return "modrinth"; }

    QWidget* createEditor(QWidget* parent) override {
        return new ModrinthEditor(parent);
    }

    void loadMetadata(QWidget* editor, const QJsonObject& md) override {
        auto* e = static_cast<ModrinthEditor*>(editor);
        if (e) e->loadMetadata(md);
    }

    QJsonObject saveMetadata(QWidget* editor) const override {
        auto* e = static_cast<ModrinthEditor*>(editor);
        return e ? e->saveMetadata() : QJsonObject();
    }
};

extern "C" __declspec(dllexport) NeoCore::IPointerEditorExtension* CreateEditorExtension() {
    return new ModrinthEditorExtension();
}
