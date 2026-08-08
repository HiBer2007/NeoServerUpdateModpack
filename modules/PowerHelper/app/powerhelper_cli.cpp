#include "powerhelper_cli.h"

#include <markdown_renderer.h>
#include <doc_group.h>

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <iostream>
#include <string>
#include <vector>

namespace PowerHelper {

namespace {

bool isJson(int argc, char* argv[])
{
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--json")
            return true;
    }
    return false;
}

bool isTocFlag(int argc, char* argv[])
{
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--toc")
            return true;
    }
    return false;
}

QString readFileOrEmpty(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void jsonBegin(std::ostream& os, const QString& cmd)
{
    os << "=====JSON-BEGIN=====\n";
    os << "{\n";
    os << "  \"category\": \"powerhelper\",\n";
    os << "  \"command\": \"" << cmd.toStdString() << "\",\n";
    os << "  \"data\": ";
}

void jsonEnd(std::ostream& os)
{
    os << "\n";
    os << "=====JSON-END=====\n";
}

QString jsonEscape(const QString& s)
{
    QString out;
    for (const QChar& c : s) {
        switch (c.unicode()) {
        case '"': out += QStringLiteral("\\\""); break;
        case '\\': out += QStringLiteral("\\\\"); break;
        case '\n': out += QStringLiteral("\\n"); break;
        case '\r': out += QStringLiteral("\\r"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        default:
            if (c.unicode() < 0x20)
                out += QStringLiteral("\\u%1")
                        .arg(static_cast<int>(c.unicode()), 4, 16,
                            QLatin1Char('0'));
            else
                out += c;
            break;
        }
    }
    return out;
}

int cmdRender(int argc, char* argv[])
{
    const bool json = isJson(argc, argv);
    std::ostream& out = json ? std::cerr : std::cout;
    if (argc < 3) {
        std::cerr << "Usage: PowerHelper render <file.md> [--json]" << std::endl;
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[2]);
    const QString md = readFileOrEmpty(path);
    if (md.isEmpty()) {
        std::cerr << "Cannot read file: " << path.toStdString() << std::endl;
        return 1;
    }
    const QString rendered = renderToTerminal(md);
    if (json) {
        jsonBegin(std::cout, "render");
        std::cout << "{\"file\": \"" << path.toStdString()
                  << "\", \"rendered\": \""
                  << jsonEscape(rendered).toStdString() << "\"}";
        jsonEnd(std::cout);
        std::cout.flush();
        out << rendered.toStdString() << "\n";
        out.flush();
        return 0;
    }
    std::cout << rendered.toStdString() << "\n";
    std::cout.flush();
    return 0;
}

int cmdToc(int argc, char* argv[])
{
    const bool json = isJson(argc, argv);
    if (argc < 3) {
        std::cerr << "Usage: PowerHelper toc <file.md> [--json]" << std::endl;
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[2]);
    const QString md = readFileOrEmpty(path);
    if (md.isEmpty()) {
        std::cerr << "Cannot read file: " << path.toStdString() << std::endl;
        return 1;
    }
    const auto toc = extractToc(md);
    if (json) {
        jsonBegin(std::cout, "toc");
        std::cout << "{\"file\": \"" << path.toStdString() << "\", \"entries\": [";
        for (int i = 0; i < toc.size(); ++i) {
            if (i)
                std::cout << ", ";
            std::cout << "{\"level\": " << toc[i].level << ", \"text\": \""
                      << toc[i].text.toStdString() << "\"}";
        }
        std::cout << "]}";
        jsonEnd(std::cout);
        return 0;
    }
    for (const auto& e : toc)
        std::cout << std::string(e.level - 1, ' ') << e.level << " "
                  << e.text.toStdString() << "\n";
    return 0;
}

int cmdGroup(int argc, char* argv[])
{
    const bool json = isJson(argc, argv);
    const bool withToc = isTocFlag(argc, argv);
    if (argc < 3) {
        std::cerr << "Usage: PowerHelper group <dir> [--toc] [--json]"
                  << std::endl;
        return 2;
    }
    const QString dir = QString::fromLocal8Bit(argv[2]);
    const auto docs = scanDocGroup(dir);
    if (docs.isEmpty()) {
        std::cerr << "No .md files found in: " << dir.toStdString()
                  << std::endl;
        return 1;
    }
    if (json) {
        jsonBegin(std::cout, "group");
        std::cout << "{\"dir\": \"" << dir.toStdString()
                  << "\", \"docs\": [";
        for (int i = 0; i < docs.size(); ++i) {
            if (i)
                std::cout << ", ";
            std::cout << "{\"path\": \"" << docs[i].relPath.toStdString()
                      << "\", \"title\": \"" << docs[i].title.toStdString()
                      << "\"";
            if (withToc) {
                std::cout << ", \"toc\": [";
                for (int h = 0; h < docs[i].toc.size(); ++h) {
                    if (h)
                        std::cout << ", ";
                    std::cout << "{\"level\": " << docs[i].toc[h].level
                              << ", \"text\": \""
                              << docs[i].toc[h].text.toStdString() << "\"}";
                }
                std::cout << "]";
            }
            std::cout << "}";
        }
        std::cout << "]}";
        jsonEnd(std::cout);
        return 0;
    }
    for (const auto& d : docs) {
        std::cout << d.relPath.toStdString() << "  |  "
                  << d.title.toStdString() << "\n";
        if (withToc) {
            for (const auto& e : d.toc)
                std::cout << "    " << std::string(e.level - 1, ' ') << e.level
                          << " " << e.text.toStdString() << "\n";
        }
    }
    return 0;
}

} // namespace

int runCli(int argc, char* argv[])
{
    const std::string verb = argv[1];
    if (verb == "render")
        return cmdRender(argc, argv);
    if (verb == "toc")
        return cmdToc(argc, argv);
    if (verb == "group")
        return cmdGroup(argc, argv);
    std::cerr << "Unknown command: '" << verb << "'. Use --help for usage."
              << std::endl;
    return 2;
}

} // namespace PowerHelper
