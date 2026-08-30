// 编辑器双版本演示: Qt 版 vs WebView2 版, 以及 merge 预览窗口。
// 启动后逐个 tab 查看效果/性能/体积对比, 确认后选定 merge 窗口所用版本。
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QDebug>

#include "code_editor_interface.h"
#include "code_editor.h"
#include "merge_preview_dialog.h"
#include "web_code_editor.h"

using namespace HiBerGUI;

static QString sampleJson()
{
    return QString::fromUtf8(
        "{\n"
        "  \"name\": \"NeoServer\",\n"
        "  \"minecraft\": \"1.21.4\",\n"
        "  \"modloader\": \"neoforge\",\n"
        "  \"modloader_version\": \"21.1.140\",\n"
        "  \"branches\": [\n"
        "    { \"name\": \"main\", \"parent\": null },\n"
        "    { \"name\": \"beta\", \"parent\": \"main\" }\n"
        "  ],\n"
        "  \"sync_policies\": {\n"
        "    \"default_folder_policy\": \"incremental_add\"\n"
        "  }\n"
        "}\n");
}

static QString sampleProps()
{
    return QString::fromUtf8(
        "# server.properties (merge sample)\n"
        "server-port=25565\n"
        "online-mode=true\n"
        "max-players=20\n"
        "motd=A NeoServer instance\n"
        "difficulty=normal\n");
}

static QString sampleMerge()
{
    return QString::fromUtf8(
        "{\n"
        "  \"name\": \"NeoServer\",\n"
        "  \"minecraft\": \"1.21.4\",\n"
        "  \"modloader\": \"neoforge\",\n"
        "  \"modloader_version\": \"21.1.140\",\n"
        "  \"branches\": [\n"
        "    { \"name\": \"main\", \"parent\": null },\n"
        "    { \"name\": \"beta\", \"parent\": \"main\" }\n"
        "  ],\n"
        "  \"sync_policies\": {\n"
        "    \"default_folder_policy\": \"incremental_add\",\n"
        "    \"files\": {\n"
        "      \"config/server.properties\": { \"mode\": \"partial\",\n"
        "        \"tracked_keys\": [\"server-port\", \"motd\"] }\n"
        "    }\n"
        "  }\n"
        "}\n");
}

class DemoWindow : public QMainWindow {
public:
    DemoWindow()
    {
        setWindowTitle(QString::fromUtf8("\u7f16\u8f91\u5668\u53cc\u7248\u672c\u6f14\u793a "
            "(Qt \u7248 vs WebView2 \u7248)"));
        resize(900, 640);

        auto* tabs = new QTabWidget(this);

        // ---- Qt 版编辑器 ----
        auto* qtPage = new QWidget(this);
        auto* qtLay = new QVBoxLayout(qtPage);
        qtLay->addWidget(new QLabel(QString::fromUtf8(
            "\u7eaf C++/Qt \u7248: \u5de6\u53f3\u5e03\u5c40 (\u884c\u53f7\u680f) + \u8bed\u6cd5\u9ad8\u4eae "
            "(VS Code \u914d\u8272) + \u533a\u57df\u6807\u8bb0 + \u6269\u5c55\u52a8\u4f5c (\u65e0 WebView \u4f9d\u8d56)"), qtPage));
        qtEditor_ = new CodeEditor(qtPage);
        qtLay->addWidget(qtEditor_, 1);
        auto* qtBtnRow = new QHBoxLayout;
        qtLangCombo_ = new QComboBox(qtPage);
        qtLangCombo_->addItems(builtinLanguages());
        auto* qtLoadJson = new QPushButton(QString::fromUtf8("\u8f7d\u5165 JSON"), qtPage);
        auto* qtLoadProps = new QPushButton(QString::fromUtf8("\u8f7d\u5165 properties"), qtPage);
        auto* qtToggleTheme = new QPushButton(QString::fromUtf8("\u5207\u6362\u6df1\u8272/\u6d45\u8272"), qtPage);
        auto* qtToggleRo = new QPushButton(QString::fromUtf8("\u53ea\u8bfb/\u53ef\u7f16\u8f91"), qtPage);
        qtTabCombo_ = new QComboBox(qtPage);
        qtTabCombo_->addItems({ QStringLiteral("2"), QStringLiteral("4") });
        qtTabCombo_->setCurrentText(QStringLiteral("4"));
        qtBtnRow->addWidget(new QLabel(QString::fromUtf8("\u8bed\u8a00:"), qtPage));
        qtBtnRow->addWidget(qtLangCombo_);
        qtBtnRow->addWidget(new QLabel(QString::fromUtf8("Tab:"), qtPage));
        qtBtnRow->addWidget(qtTabCombo_);
        qtBtnRow->addWidget(qtLoadJson);
        qtBtnRow->addWidget(qtLoadProps);
        qtBtnRow->addWidget(qtToggleTheme);
        qtBtnRow->addWidget(qtToggleRo);
        qtBtnRow->addStretch(1);
        qtLay->addLayout(qtBtnRow);
        connect(qtLangCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                qtEditor_->setLanguage(qtLangCombo_->currentText());
            });
        connect(qtTabCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                qtEditor_->setTabWidth(qtTabCombo_->currentText().toInt());
            });
        connect(qtLoadJson, &QPushButton::clicked, this, [this]() {
            qtEditor_->setLanguage(QStringLiteral("json"));
            qtLangCombo_->setCurrentText(QStringLiteral("json"));
            qtEditor_->setPlainText(sampleJson());
        });
        connect(qtLoadProps, &QPushButton::clicked, this, [this]() {
            qtEditor_->setLanguage(QStringLiteral("properties"));
            qtLangCombo_->setCurrentText(QStringLiteral("properties"));
            qtEditor_->setPlainText(sampleProps());
        });
        connect(qtToggleTheme, &QPushButton::clicked, this, [this]() {
            qtDark_ = !qtDark_;
            qtEditor_->setDarkMode(qtDark_);
        });
        connect(qtToggleRo, &QPushButton::clicked, this, [this]() {
            qtRo_ = !qtRo_;
            qtEditor_->setReadOnly(qtRo_);
        });
        qtEditor_->setLanguage(QStringLiteral("json"));
        qtEditor_->setPlainText(sampleJson());
        qtEditor_->setRegionHighlights({
            { 3, 4, QStringLiteral("#2f6b31"), QStringLiteral("tracked") }
        });
        qtEditor_->addAction({ "fmt", QString::fromUtf8("\u7f29\u8fdb\u68c0\u67e5"),
            QString::fromUtf8("\u6269\u5c55\u52a8\u4f5c\u793a\u4f8b"),
            [](QWidget* w) { qDebug() << "extension action on" << w; } });

        // ---- Web 版编辑器 ----
        auto* webPage = new QWidget(this);
        auto* webLay = new QVBoxLayout(webPage);
        webLay->addWidget(new QLabel(QString::fromUtf8(
            "WebView2 \u7248: \u540c\u4e00 ICodeEditor \u63a5\u53e3, "
            "\u9700 WebView2 \u8fd0\u884c\u65f6 (\u4ec5\u94fe\u63a5\u672c\u5e93\u624d\u4f9d\u8d56 WebView2Loader.dll)"), webPage));
        webEditor_ = new WebCodeEditor(webPage);
        webLay->addWidget(webEditor_, 1);
        auto* webBtnRow = new QHBoxLayout;
        webLangCombo_ = new QComboBox(webPage);
        webLangCombo_->addItems(builtinLanguages());
        auto* webLoadJson = new QPushButton(QString::fromUtf8("\u8f7d\u5165 JSON"), webPage);
        auto* webLoadProps = new QPushButton(QString::fromUtf8("\u8f7d\u5165 properties"), webPage);
        auto* webToggleTheme = new QPushButton(QString::fromUtf8("\u5207\u6362\u6df1\u8272/\u6d45\u8272"), webPage);
        auto* webToggleRo = new QPushButton(QString::fromUtf8("\u53ea\u8bfb/\u53ef\u7f16\u8f91"), webPage);
        webTabCombo_ = new QComboBox(webPage);
        webTabCombo_->addItems({ QStringLiteral("2"), QStringLiteral("4") });
        webTabCombo_->setCurrentText(QStringLiteral("4"));
        webBtnRow->addWidget(new QLabel(QString::fromUtf8("\u8bed\u8a00:"), webPage));
        webBtnRow->addWidget(webLangCombo_);
        webBtnRow->addWidget(new QLabel(QString::fromUtf8("Tab:"), webPage));
        webBtnRow->addWidget(webTabCombo_);
        webBtnRow->addWidget(webLoadJson);
        webBtnRow->addWidget(webLoadProps);
        webBtnRow->addWidget(webToggleTheme);
        webBtnRow->addWidget(webToggleRo);
        webBtnRow->addStretch(1);
        webLay->addLayout(webBtnRow);
        connect(webLangCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                webEditor_->setLanguage(webLangCombo_->currentText());
            });
        connect(webTabCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                webEditor_->setTabWidth(webTabCombo_->currentText().toInt());
            });
        connect(webLoadJson, &QPushButton::clicked, this, [this]() {
            webEditor_->setLanguage(QStringLiteral("json"));
            webLangCombo_->setCurrentText(QStringLiteral("json"));
            webEditor_->setPlainText(sampleJson());
        });
        connect(webLoadProps, &QPushButton::clicked, this, [this]() {
            webEditor_->setLanguage(QStringLiteral("properties"));
            webLangCombo_->setCurrentText(QStringLiteral("properties"));
            webEditor_->setPlainText(sampleProps());
        });
        connect(webToggleTheme, &QPushButton::clicked, this, [this]() {
            webDark_ = !webDark_;
            webEditor_->setDarkMode(webDark_);
        });
        connect(webToggleRo, &QPushButton::clicked, this, [this]() {
            webRo_ = !webRo_;
            webEditor_->setReadOnly(webRo_);
        });
        webEditor_->setLanguage(QStringLiteral("json"));
        webEditor_->setPlainText(sampleJson());
        webEditor_->setRegionHighlights({
            { 3, 4, QStringLiteral("#2f6b31"), QStringLiteral("tracked") }
        });

        // ---- merge 预览窗口 (两版) ----
        auto* mergePage = new QWidget(this);
        auto* mLay = new QVBoxLayout(mergePage);
        mLay->addWidget(new QLabel(QString::fromUtf8(
            "\u9884\u89c8 merge \u7a97\u53e3: \u5df2\u9009\u5b9a Qt \u7248\u5e94\u7528\u4e8e\u4ea7\u54c1 "
            "(ConfigFileEditor); Web \u7248\u4ec5\u4f5c\u5bf9\u6bd4\u7528"), mergePage));
        auto* mBtnRow = new QHBoxLayout;
        auto* qtMergeBtn = new QPushButton(QString::fromUtf8("\u6253\u5f00 Qt \u7248 merge \u9884\u89c8"), mergePage);
        auto* webMergeBtn = new QPushButton(QString::fromUtf8("\u6253\u5f00 Web \u7248 merge \u9884\u89c8"), mergePage);
        mBtnRow->addWidget(qtMergeBtn);
        mBtnRow->addWidget(webMergeBtn);
        mBtnRow->addStretch(1);
        mLay->addLayout(mBtnRow);
        mLay->addStretch(1);
        connect(qtMergeBtn, &QPushButton::clicked, this, [this]() {
            MergePreviewDialog dlg(CodeEditorKind::Qt, this);
            if (dlg.editor()) {
                dlg.editor()->setDarkMode(qtDark_);
            }
            dlg.setContent(sampleMerge(),
                QString::fromUtf8(
                    "\u9ec4\u8272\u6807\u8bb0 = \u6e90\u6587\u4ef6\u5168\u91cf\u8986\u76d6\u7684\u884c\uff1b"
                    "\u7eff\u8272\u6807\u8bb0 = \u8ffd\u8e2a\u7684\u952e\u503c\u5bf9 (Qt \u7248\u7f16\u8f91\u5668)"),
                QStringLiteral("json"),
                { { 2, 4, QStringLiteral("#8a6d1a"), QStringLiteral("overwrite"),
                      -1, -1, QStringLiteral("#f7e8a8") },
                  { 5, 5, QStringLiteral("#2f6b31"), QStringLiteral("tracked"),
                      2, 33, QStringLiteral("#b7e4c7") } });
            dlg.exec();
        });
        connect(webMergeBtn, &QPushButton::clicked, this, [this]() {
            MergePreviewDialog dlg(CodeEditorKind::Web, this);
            if (dlg.editor()) {
                dlg.editor()->setDarkMode(webDark_);
            }
            dlg.setContent(sampleMerge(),
                QString::fromUtf8(
                    "\u9ec4\u8272\u6807\u8bb0 = \u6e90\u6587\u4ef6\u5168\u91cf\u8986\u76d6\u7684\u884c\uff1b"
                    "\u7eff\u8272\u6807\u8bb0 = \u8ffd\u8e2a\u7684\u952e\u503c\u5bf9 (Web \u7248\u7f16\u8f91\u5668)"),
                QStringLiteral("json"),
                { { 2, 4, QStringLiteral("#8a6d1a"), QStringLiteral("overwrite"),
                      -1, -1, QStringLiteral("#f7e8a8") },
                  { 5, 5, QStringLiteral("#2f6b31"), QStringLiteral("tracked"),
                      2, 33, QStringLiteral("#b7e4c7") } });
            dlg.exec();
        });

        tabs->addTab(qtPage, QString::fromUtf8("Qt \u7248\u7f16\u8f91\u5668"));
        tabs->addTab(webPage, QString::fromUtf8("WebView2 \u7248\u7f16\u8f91\u5668"));
        tabs->addTab(mergePage, QString::fromUtf8("merge \u9884\u89c8\u5bf9\u6bd4"));
        setCentralWidget(tabs);


        // 性能展示: Web 版就绪耗时 (页面激活时)
        QTimer::singleShot(500, this, [this]() {
            if (webEditor_ && webEditor_->usingWebView()) {
                statusBar()->showMessage(
                    QString::fromUtf8("WebView2 \u7248\u5c31\u7eea\u3002"), 6000);
            }
        });
    }

private:
    CodeEditor* qtEditor_ = nullptr;
    WebCodeEditor* webEditor_ = nullptr;
    QComboBox* qtLangCombo_ = nullptr;
    QComboBox* webLangCombo_ = nullptr;
    QComboBox* qtTabCombo_ = nullptr;
    QComboBox* webTabCombo_ = nullptr;
    bool qtDark_ = true;
    bool webDark_ = true;
    bool qtRo_ = false;
    bool webRo_ = false;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // 注册 Web 工厂 (仅链接 HiBerGUIWebView2 的程序可注册)
    registerCodeEditorFactory(CodeEditorKind::Web,
        [](QWidget* parent) -> ICodeEditor* {
            return createWebCodeEditor(parent);
        });

    DemoWindow w;
    w.show();
    // --web: 启动即切到 WebView2 页 (便于诊断与对比)
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--web") == 0) {
            if (auto* tw = w.findChild<QTabWidget*>()) {
                tw->setCurrentIndex(1);
            }
        }
    }
    return app.exec();
}
