#include "build_tool_window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTime>
#include <QKeyEvent>
#include <QScrollBar>

namespace GUIWorker {

BuildToolWindow::BuildToolWindow(QWidget* parent)
    : QWidget(parent)
    , historyIndex_(-1)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel("构建工具", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    outputEdit_ = new QTextEdit(this);
    outputEdit_->setReadOnly(true);
    outputEdit_->setFont(QFont("Consolas", 10));
    outputEdit_->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #333;"
        "  border-radius: 4px;"
        "  padding: 6px;"
        "}");
    layout->addWidget(outputEdit_, 1);

    auto* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    commandEdit_ = new QLineEdit(this);
    commandEdit_->setPlaceholderText("输入命令 (help 查看帮助)...");
    commandEdit_->setMinimumHeight(32);
    commandEdit_->setFont(QFont("Consolas", 10));
    commandEdit_->installEventFilter(this);

    executeBtn_ = new QPushButton("执行", this);
    executeBtn_->setMinimumWidth(80);
    executeBtn_->setMinimumHeight(32);

    inputLayout->addWidget(commandEdit_, 1);
    inputLayout->addWidget(executeBtn_);
    layout->addLayout(inputLayout);

    auto* hintLabel = new QLabel(
        "支持的命令: status | build <分支> | export <格式> <路径> | help", this);
    hintLabel->setStyleSheet("color: #888; font-size: 11px; padding-left: 4px;");
    layout->addWidget(hintLabel);

    connect(executeBtn_, &QPushButton::clicked,
        this, &BuildToolWindow::onExecute);
    connect(commandEdit_, &QLineEdit::returnPressed,
        this, &BuildToolWindow::onExecute);

    appendLog("info", "Build tool started. Type help for available commands.");
}

void BuildToolWindow::appendOutput(const QString& text)
{
    outputEdit_->append(text);
    QScrollBar* sb = outputEdit_->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void BuildToolWindow::clearOutput()
{
    outputEdit_->clear();
}

void BuildToolWindow::onExecute()
{
    QString cmd = commandEdit_->text().trimmed();
    if (cmd.isEmpty()) {
        return;
    }

    commandHistory_.append(cmd);
    historyIndex_ = -1;
    commandEdit_->clear();
    currentInput_.clear();

    executeCommand(cmd);
}

bool BuildToolWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == commandEdit_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Up) {
            navigateHistory(1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            navigateHistory(-1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void BuildToolWindow::navigateHistory(int direction)
{
    if (commandHistory_.isEmpty()) {
        return;
    }

    if (direction > 0) {
        if (historyIndex_ == -1) {
            currentInput_ = commandEdit_->text();
        }
        if (historyIndex_ < commandHistory_.size() - 1) {
            historyIndex_++;
        }
    } else {
        if (historyIndex_ > 0) {
            historyIndex_--;
        } else if (historyIndex_ == 0) {
            historyIndex_ = -1;
            commandEdit_->setText(currentInput_);
            return;
        }
    }

    if (historyIndex_ >= 0 && historyIndex_ < commandHistory_.size()) {
        int idx = commandHistory_.size() - 1 - historyIndex_;
        commandEdit_->setText(commandHistory_.at(idx));
    }
}

void BuildToolWindow::executeCommand(const QString& cmd)
{
    appendLog("cmd", "> " + cmd);
    emit commandEntered(cmd);

    QString lower = cmd.toLower().trimmed();

    if (lower == "help" || lower == "?") {
        showHelp();
        return;
    }

    if (lower == "status") {
    appendLog("info", "System status: ready");
    appendLog("info", "NSUM Build Tool v1.0.0");
        return;
    }

    if (lower.startsWith("build ")) {
        QString branch = cmd.mid(6).trimmed();
        if (branch.isEmpty()) {
    appendLog("warn", "Usage: build <branch>");
        } else {
    appendLog("info", QString("Start building branch: %1").arg(branch));
    appendLog("info", "Submitting changes, please wait...");
        }
        return;
    }

    if (lower.startsWith("export ")) {
        QString args = cmd.mid(7).trimmed();
        if (args.isEmpty()) {
    appendLog("warn", "Usage: export <format> <path>");
        } else {
    appendLog("info", QString("Exporting: %1").arg(args));
        }
        return;
    }

    if (lower == "clear" || lower == "cls") {
        clearOutput();
    appendLog("info", "Export complete");
        return;
    }

    if (lower == "logs") {
    appendLog("info", "Showing recent logs (see log file for full history)");
        return;
    }

    appendLog("warn", QString("Unknown command: %1 (type help for usage)").arg(cmd));
}

void BuildToolWindow::appendLog(const QString& prefix, const QString& text)
{
    QString timeStr = QTime::currentTime().toString("HH:mm:ss");

    QString color = "#d4d4d4";
    if (prefix == "cmd") {
        color = "#569cd6";
    } else if (prefix == "info") {
        color = "#6a9955";
    } else if (prefix == "warn") {
        color = "#ce9178";
    } else if (prefix == "err") {
        color = "#f44747";
    }

    outputEdit_->append(
        QString("<span style='color:%1'>[%2] %3</span>")
            .arg(color, timeStr, text));
}

void BuildToolWindow::showHelp()
{
    appendLog("info", "========== Build Tool Help ==========");
    appendLog("info", "  status         show current status");
    appendLog("info", "  build <branch>  build specified branch");
    appendLog("info", "  export <format> <path>  export modpack");
    appendLog("info", "  help / ?       show this help");
    appendLog("info", "  clear / cls    clear console");
    appendLog("info", "  logs           show log messages");
    appendLog("info", "==============================");
}

} // namespace GUIWorker

#include "build_tool_window.moc"


