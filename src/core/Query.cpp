#include "core/Query.h"

#include <cctype>

namespace fe {

namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// split on whitespace, quotes group stuff together and get dropped
std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuote = false;
    for (char c : text) {
        if (c == '"') {
            inQuote = !inQuote;
        } else if (!inQuote && std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) tokens.push_back(std::move(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(std::move(cur));
    return tokens;
}

}  // namespace

Query parseQuery(std::string_view text, const SearchOptions& options) {
    Query q;
    q.matchCase = options.matchCase;
    q.wholeWord = options.wholeWord;
    q.matchPath = options.matchPath;
    q.filesOnly = options.kind == SearchOptions::Kind::Files;
    q.foldersOnly = options.kind == SearchOptions::Kind::Folders;

    for (const std::string& raw : tokenize(text)) {
        std::string_view tok = raw;
        bool negate = false;
        if (tok.front() == '!') {
            negate = true;
            tok.remove_prefix(1);
            if (tok.empty()) continue;
        }

        std::string lower = toLower(tok);

        if (lower.starts_with("ext:")) {
            std::string_view list = std::string_view(lower).substr(4);
            while (!list.empty()) {
                size_t semi = list.find(';');
                std::string_view ext = list.substr(0, semi);
                if (!ext.empty() && ext.front() == '.') ext.remove_prefix(1);
                if (!ext.empty()) q.extensions.emplace_back(ext);
                if (semi == std::string_view::npos) break;
                list.remove_prefix(semi + 1);
            }
        } else if (lower == "file:") {
            q.filesOnly = true;
        } else if (lower == "folder:") {
            q.foldersOnly = true;
        } else {
            if (tok.find('/') != std::string_view::npos) q.matchPath = true;
            (negate ? q.exclude : q.include).push_back(q.matchCase ? std::string(tok) : std::move(lower));
        }
    }
    return q;
}

}  // namespace fe
