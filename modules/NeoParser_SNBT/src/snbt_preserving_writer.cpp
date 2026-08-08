#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace SnbtPreserving {

enum class TokenType {
    CommentLine,
    KeyValueLine,
    StructuralLine,
    BlankLine,
    Unknown
};

struct TokenLine {
    TokenType type = TokenType::Unknown;
    int line_number = 0;
    std::string raw_text;
    std::string indent;
    std::string key;
    std::string value;
    std::string trailing_comment;
};

static std::string rtrim(const std::string& s)
{
    auto end = s.find_last_not_of(" \t\r");
    if (end == std::string::npos) {
        return {};
    }
    return s.substr(0, end + 1);
}

static std::string ltrim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return {};
    }
    return s.substr(start);
}

static std::string trim(const std::string& s)
{
    return ltrim(rtrim(s));
}

static TokenType classify_line(const std::string& trimmed, TokenLine& tl)
{
    if (trimmed.empty()) {
        return TokenType::BlankLine;
    }

    char first = trimmed.front();

    if (first == '#') {
        return TokenType::CommentLine;
    }

    if (first == '{' || first == '}' || first == '[' || first == ']') {
        if (trimmed.size() == 1 ||
            (trimmed.size() > 1 && trimmed.find_first_not_of("{}[], \t") == std::string::npos)) {
            return TokenType::StructuralLine;
        }
    }

    size_t colon_pos = std::string::npos;
    bool in_string = false;
    char string_char = 0;
    int depth = 0;

    for (size_t i = 0; i < trimmed.size(); ++i) {
        char c = trimmed[i];
        if (in_string) {
            if (c == '\\' && i + 1 < trimmed.size()) {
                ++i;
                continue;
            }
            if (c == string_char) {
                in_string = false;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            in_string = true;
            string_char = c;
            continue;
        }
        if (c == '{' || c == '[') {
            ++depth;
            continue;
        }
        if (c == '}' || c == ']') {
            --depth;
            continue;
        }
        if (c == ':' && depth == 0) {
            colon_pos = i;
            break;
        }
    }

    if (colon_pos != std::string::npos) {
        std::string key_part = trimmed.substr(0, colon_pos);
        std::string val_part = trimmed.substr(colon_pos + 1);

        std::string raw_key = trim(key_part);
        if (!raw_key.empty() && raw_key.front() == '"' && raw_key.back() == '"' && raw_key.size() >= 2) {
            raw_key = raw_key.substr(1, raw_key.size() - 2);
        } else if (!raw_key.empty() && raw_key.front() == '\'' && raw_key.back() == '\'' && raw_key.size() >= 2) {
            raw_key = raw_key.substr(1, raw_key.size() - 2);
        }

        tl.key = raw_key;
        tl.value = val_part;
        return TokenType::KeyValueLine;
    }

    return TokenType::Unknown;
}

static std::vector<TokenLine> tokenize(const std::string& content)
{
    std::vector<TokenLine> tokens;
    std::istringstream stream(content);
    std::string line;

    int line_num = 0;
    while (std::getline(stream, line)) {
        ++line_num;

        TokenLine tl;
        tl.line_number = line_num;
        tl.raw_text = line;

        size_t content_start = line.find_first_not_of(" \t");
        if (content_start != std::string::npos) {
            tl.indent = line.substr(0, content_start);
            tl.type = classify_line(line.substr(content_start), tl);
        } else {
            tl.type = TokenType::BlankLine;
        }

        tokens.push_back(std::move(tl));
    }

    return tokens;
}

static std::unordered_map<std::string, std::string> build_remote_map(
    const std::vector<TokenLine>& remote_tokens)
{
    std::unordered_map<std::string, std::string> result;
    for (const auto& t : remote_tokens) {
        if (t.type == TokenType::KeyValueLine && !t.key.empty()) {
            result[t.key] = t.value;
        }
    }
    return result;
}

std::string merge(const std::string& localContent,
                  const std::string& remoteContent,
                  const std::vector<std::string>& trackedKeys)
{
    auto local_tokens = tokenize(localContent);
    auto remote_tokens = tokenize(remoteContent);

    auto remote_values = build_remote_map(remote_tokens);

    std::unordered_set<std::string> tracked;
    for (const auto& k : trackedKeys) {
        tracked.insert(k);
    }

    std::ostringstream result;

    for (size_t i = 0; i < local_tokens.size(); ++i) {
        auto& t = local_tokens[i];

        if (t.type == TokenType::KeyValueLine && !t.key.empty() && tracked.count(t.key)) {
            auto it = remote_values.find(t.key);
            if (it != remote_values.end()) {
                std::string remote_val = it->second;

                size_t vs = remote_val.find_first_not_of(" \t");
                if (vs != std::string::npos) {
                    remote_val = remote_val.substr(vs);
                }

                result << t.indent << t.key << ": " << remote_val;

                bool is_last = (i == local_tokens.size() - 1);
                if (!is_last) {
                    result << "\n";
                }
                continue;
            }
        }

        result << t.raw_text;

        bool is_last = (i == local_tokens.size() - 1);
        if (!is_last) {
            result << "\n";
        }
    }

    if (!localContent.empty() && localContent.back() == '\n') {
        result << "\n";
    }

    return result.str();
}

} // namespace SnbtPreserving
