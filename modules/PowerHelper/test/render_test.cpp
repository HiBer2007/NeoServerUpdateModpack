#include <markdown_renderer.h>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qWarning() << "usage: ph_rendertest <file.md> [html-out]";
        return 2;
    }
    QFile f(QString::fromLocal8Bit(argv[1]));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "cannot open" << argv[1];
        return 1;
    }
    const QString md = QString::fromUtf8(f.readAll());

    QTextStream out(stdout);
    out << "===== TOC =====\n";
    const auto toc = PowerHelper::extractToc(md);
    for (const auto& e : toc)
        out << e.level << " " << e.text << "\n";
    out << "===== TERMINAL =====\n";
    out << PowerHelper::renderToTerminal(md) << "\n";
    out.flush();

    if (argc > 2) {
        QFile outF(QString::fromLocal8Bit(argv[2]));
        if (outF.open(QIODevice::WriteOnly))
            outF.write(PowerHelper::renderToHtml(md).toUtf8());
    }
    return 0;
}
