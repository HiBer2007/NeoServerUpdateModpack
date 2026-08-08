#include "reader_window.h"

#include <markdown_viewer.h>

#include <QSplitter>
#include <QTreeWidget>
#include <QStatusBar>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QFont>

namespace PowerHelper {

ReaderWindow::ReaderWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUI();
    resize(1024, 720);
    setWindowTitle(QStringLiteral("PowerHelper"));
}

void ReaderWindow::buildUI()
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);

    tocTree_ = new QTreeWidget(splitter);
    tocTree_->setHeaderHidden(true);
    tocTree_->setMinimumWidth(240);
    tocTree_->setMaximumWidth(420);

    viewer_ = new MarkdownViewer(splitter);
    splitter->addWidget(tocTree_);
    splitter->addWidget(viewer_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 280, 744 });

    statusBar()->showMessage(QStringLiteral("就绪"));

    connect(tocTree_, &QTreeWidget::currentItemChanged,
        this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
            if (!item)
                return;
            const int doc = item->data(0, Qt::UserRole).toInt();
            const int hd = item->data(0, Qt::UserRole + 1).toInt();
            if (hd < 0)
                loadDoc(doc);
            else
                loadHeading(doc, hd);
        });
    connect(viewer_, &MarkdownViewer::openFileRequested,
        this, &ReaderWindow::openDocPath);
}

void ReaderWindow::openFile(const QString& path)
{
    DocFileInfo info;
    info.absPath = path;
    info.relPath = QFileInfo(path).fileName();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
        info.toc = extractToc(QString::fromUtf8(f.readAll()));
    info.title = QFileInfo(path).fileName();
    docs_.clear();
    docs_.append(info);
    currentFile_ = path;
    fillTree();
    loadDoc(0);
    setWindowTitle(QStringLiteral("PowerHelper - %1").arg(info.title));
}

void ReaderWindow::openGroup(const QString& dir)
{
    docs_ = scanDocGroup(dir);
    if (docs_.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("目录中未找到 .md 文档"));
        return;
    }
    currentFile_.clear();
    fillTree();
    loadDoc(0);
    setWindowTitle(QStringLiteral("PowerHelper - %1")
            .arg(QFileInfo(dir).fileName()));
}

void ReaderWindow::fillTree()
{
    tocTree_->clear();
    for (int d = 0; d < docs_.size(); ++d) {
        const DocFileInfo& info = docs_[d];
        auto* docItem = new QTreeWidgetItem(tocTree_);
        docItem->setText(0, info.title);
        docItem->setToolTip(0, info.relPath);
        docItem->setData(0, Qt::UserRole, d);
        docItem->setData(0, Qt::UserRole + 1, -1);
        QFont f = docItem->font(0);
        f.setBold(true);
        docItem->setFont(0, f);
        for (int h = 0; h < info.toc.size(); ++h) {
            const TocEntry& e = info.toc[h];
            auto* headItem = new QTreeWidgetItem(docItem);
            headItem->setText(0,
                QString(e.level - 1, QLatin1Char(' ')) + e.text);
            headItem->setData(0, Qt::UserRole, d);
            headItem->setData(0, Qt::UserRole + 1, h);
        }
    }
    tocTree_->expandAll();
}

void ReaderWindow::loadDoc(int docIndex)
{
    if (docIndex < 0 || docIndex >= docs_.size())
        return;
    const DocFileInfo& info = docs_[docIndex];
    currentFile_ = info.absPath;
    viewer_->loadFile(info.absPath);
    statusBar()->showMessage(
        QStringLiteral("%1  |  %2 个标题")
            .arg(info.relPath)
            .arg(info.toc.size()));
    setWindowTitle(QStringLiteral("PowerHelper - %1").arg(info.title));
}

void ReaderWindow::loadHeading(int docIndex, int headingIndex)
{
    if (docIndex < 0 || docIndex >= docs_.size())
        return;
    if (currentFile_ != docs_[docIndex].absPath)
        loadDoc(docIndex);
    viewer_->scrollToHeading(headingIndex);
}

void ReaderWindow::scrollToHeadingText(const QString& text)
{
    if (text.isEmpty())
        return;
    const auto toc = viewer_->toc();
    for (int i = 0; i < toc.size(); ++i) {
        if (toc[i].text.contains(text)) {
            viewer_->scrollToHeading(i);
            return;
        }
    }
}

void ReaderWindow::openDocPath(const QString& path, const QString& anchor)
{
    const QString canon = QFileInfo(path).canonicalFilePath();
    for (int d = 0; d < docs_.size(); ++d) {
        if (QFileInfo(docs_[d].absPath).canonicalFilePath() == canon) {
            loadDoc(d);
            if (!anchor.isEmpty())
                scrollToHeadingText(anchor);
            return;
        }
    }
    DocFileInfo info;
    info.absPath = path;
    info.relPath = QFileInfo(path).fileName();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
        info.toc = extractToc(QString::fromUtf8(f.readAll()));
    info.title = QFileInfo(path).fileName();
    docs_ = { info };
    fillTree();
    loadDoc(0);
    if (!anchor.isEmpty())
        scrollToHeadingText(anchor);
}

} // namespace PowerHelper
