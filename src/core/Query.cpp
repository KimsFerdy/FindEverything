#include "core/Query.h"

#include <regex.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>

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

// pulled from what Everything ships as its default filters, trimmed down
const std::vector<std::string>& filterExtensionList(Filter f) {
    static const std::vector<std::string> kAudio = {"mp3", "flac", "ogg", "oga", "opus", "wav",  "m4a", "aac",
                                                    "wma", "aiff", "aif", "ape", "mid",  "midi", "mka"};
    static const std::vector<std::string> kVideo = {"mp4", "mkv",  "webm", "avi", "mov", "wmv",  "flv", "m4v",
                                                    "mpg", "mpeg", "3gp",  "ogv", "ts",  "m2ts", "vob"};
    static const std::vector<std::string> kPictures = {"jpg", "jpeg", "png", "gif",  "bmp",  "webp", "svg",
                                                       "tif", "tiff", "ico", "heic", "heif", "avif", "raw",
                                                       "cr2", "nef",  "arw", "dng",  "psd",  "xcf",  "kra"};
    static const std::vector<std::string> kDocuments = {"pdf", "doc",  "docx", "odt", "rtf", "txt", "md",
                                                        "tex", "xls",  "xlsx", "ods", "csv", "ppt", "pptx",
                                                        "odp", "epub", "djvu", "odg", "org", "rst"};
    static const std::vector<std::string> kCode = {
        "c",   "h",    "cpp", "hpp",  "cc",   "cxx",   "hh",  "py",   "js",  "ts",    "jsx", "tsx", "java", "kt",
        "go",  "rs",   "rb",  "php",  "cs",   "swift", "sh",  "bash", "zsh", "fish",  "lua", "pl",  "r",    "scala",
        "sql", "html", "css", "scss", "json", "yaml",  "yml", "toml", "xml", "cmake", "mk",  "glsl"};
    static const std::vector<std::string> kArchives = {"zip",  "tar", "gz",  "tgz", "bz2",  "xz",     "zst",
                                                       "7z",   "rar", "iso", "deb", "rpm",  "jar",    "lz",
                                                       "lzma", "cab", "img", "dmg", "snap", "flatpak"};
    static const std::vector<std::string> kExecutables = {"appimage", "run", "bin"};  // + the exec bit
    static const std::vector<std::string> kNone;
    switch (f) {
        case Filter::Audio:
            return kAudio;
        case Filter::Video:
            return kVideo;
        case Filter::Pictures:
            return kPictures;
        case Filter::Documents:
            return kDocuments;
        case Filter::Code:
            return kCode;
        case Filter::Archives:
            return kArchives;
        case Filter::Executables:
            return kExecutables;
        default:
            return kNone;
    }
}

// "10mb" -> 10485760. false on garbage
bool parseSize(std::string_view s, uint64_t& out) {
    char* end = nullptr;
    const std::string num(s);
    const double v = std::strtod(num.c_str(), &end);
    if (end == num.c_str() || v < 0) return false;
    const std::string unit = toLower(std::string_view(end));
    double mul = 1;
    if (unit.empty() || unit == "b")
        mul = 1;
    else if (unit == "k" || unit == "kb")
        mul = 1024.0;
    else if (unit == "m" || unit == "mb")
        mul = 1024.0 * 1024;
    else if (unit == "g" || unit == "gb")
        mul = 1024.0 * 1024 * 1024;
    else if (unit == "t" || unit == "tb")
        mul = 1024.0 * 1024 * 1024 * 1024;
    else
        return false;
    out = static_cast<uint64_t>(std::llround(v * mul));
    return true;
}

int64_t localMidnight(int64_t t) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    localtime_r(&tt, &tm);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
}

// "2026-10-17" -> [that day's local midnight, next midnight)
bool parseDay(std::string_view s, int64_t& start, int64_t& end) {
    int y, m, d;
    char tail;
    if (std::sscanf(std::string(s).c_str(), "%d-%d-%d%c", &y, &m, &d, &tail) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    std::tm tm{};
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_isdst = -1;
    start = std::mktime(&tm);
    tm.tm_mday += 1;
    tm.tm_isdst = -1;
    end = std::mktime(&tm);
    return start != -1;
}

// a range for one dm: value. keywords look back from now
bool parseDateRange(std::string_view v, int64_t now, int64_t& lo, int64_t& hi) {
    constexpr int64_t kDay = 86400;
    const int64_t today = localMidnight(now);
    const std::string s = toLower(v);
    lo = std::numeric_limits<int64_t>::min();
    hi = std::numeric_limits<int64_t>::max();
    if (s == "today") return lo = today, true;
    if (s == "yesterday") return lo = today - kDay, hi = today, true;
    if (s == "week") return lo = now - 7 * kDay, true;
    if (s == "month") return lo = now - 30 * kDay, true;
    if (s == "year") return lo = now - 365 * kDay, true;

    int64_t a, b, c, d;
    if (size_t dots = s.find(".."); dots != std::string::npos) {
        if (!parseDay(std::string_view(s).substr(0, dots), a, b) ||
            !parseDay(std::string_view(s).substr(dots + 2), c, d))
            return false;
        return lo = a, hi = d, true;  // both days included
    }
    if (s.starts_with(">=")) return parseDay(std::string_view(s).substr(2), a, b) && (lo = a, true);
    if (s.starts_with("<=")) return parseDay(std::string_view(s).substr(2), a, b) && (hi = b, true);
    if (s.starts_with(">")) return parseDay(std::string_view(s).substr(1), a, b) && (lo = b, true);
    if (s.starts_with("<")) return parseDay(std::string_view(s).substr(1), a, b) && (hi = a, true);
    return parseDay(s, lo, hi);
}

bool parseSizeRange(std::string_view v, uint64_t& lo, uint64_t& hi) {
    lo = 0;
    hi = std::numeric_limits<uint64_t>::max();
    uint64_t a, b;
    if (size_t dots = v.find(".."); dots != std::string_view::npos) {
        if (!parseSize(v.substr(0, dots), a) || !parseSize(v.substr(dots + 2), b)) return false;
        return lo = a, hi = b + 1, true;
    }
    if (v.starts_with(">=")) return parseSize(v.substr(2), a) && (lo = a, true);
    if (v.starts_with("<=")) return parseSize(v.substr(2), a) && (hi = a + 1, true);
    if (v.starts_with(">")) return parseSize(v.substr(1), a) && (lo = a + 1, true);
    if (v.starts_with("<")) return parseSize(v.substr(1), a) && (hi = a, true);
    return parseSize(v, a) && (lo = a, hi = a + 1, true);
}

bool validRegex(const std::string& pattern, bool matchCase) {
    regex_t re;
    if (regcomp(&re, pattern.c_str(), REG_EXTENDED | REG_NOSUB | (matchCase ? 0 : REG_ICASE)) != 0) return false;
    regfree(&re);
    return true;
}

}  // namespace

const char* filterName(Filter filter) {
    static const char* kNames[] = {"Everything", "Audio", "Video",    "Pictures",
                                   "Documents",  "Code",  "Archives", "Executables"};
    return kNames[static_cast<int>(filter)];
}

bool globMatch(std::string_view pat, std::string_view str) {
    // iterative with one backtrack point, linear-ish, no recursion
    size_t p = 0, s = 0, starP = std::string_view::npos, starS = 0;
    while (s < str.size()) {
        if (p < pat.size() && (pat[p] == '?' || pat[p] == str[s])) {
            ++p;
            ++s;
        } else if (p < pat.size() && pat[p] == '*') {
            starP = p++;
            starS = s;
        } else if (starP != std::string_view::npos) {
            p = starP + 1;
            s = ++starS;
        } else {
            return false;
        }
    }
    while (p < pat.size() && pat[p] == '*') ++p;
    return p == pat.size();
}

Query parseQuery(std::string_view text, const SearchOptions& options, int64_t now) {
    if (now == 0) now = static_cast<int64_t>(std::time(nullptr));
    Query q;
    q.matchCase = options.matchCase;
    q.wholeWord = options.wholeWord;
    q.matchPath = options.matchPath;
    q.filesOnly = options.kind == SearchOptions::Kind::Files;
    q.foldersOnly = options.kind == SearchOptions::Kind::Folders;
    q.filterExtensions = filterExtensionList(options.filter);
    q.filterExecutables = options.filter == Filter::Executables;

    auto fail = [&](const std::string& what) {
        if (q.error.empty()) q.error = what;
    };

    for (const std::string& raw : tokenize(text)) {
        std::string_view tok = raw;
        bool negate = false;
        if (tok.front() == '!') {
            negate = true;
            tok.remove_prefix(1);
            if (tok.empty()) continue;
        }

        std::string lower = toLower(tok);
        auto value = [&](size_t prefix) { return std::string(tok.substr(prefix)); };

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
        } else if (lower == "path:") {
            q.matchPath = true;
        } else if (lower.starts_with("regex:")) {
            const std::string pattern = value(6);
            if (pattern.empty()) continue;
            if (!validRegex(pattern, q.matchCase)) {
                fail("invalid regex: " + pattern);
                continue;
            }
            (negate ? q.excludeRegex : q.includeRegex).push_back(pattern);
        } else if (lower.starts_with("size:")) {
            uint64_t lo, hi;
            if (!parseSizeRange(std::string_view(lower).substr(5), lo, hi)) {
                fail("can't read size: " + value(5) + "  (try size:>10mb, size:1mb..1gb)");
                continue;
            }
            q.minSize = std::max(q.minSize, lo);
            q.maxSize = std::min(q.maxSize, hi);
        } else if (lower.starts_with("dm:") || lower.starts_with("datemodified:")) {
            const size_t colon = lower.find(':');
            int64_t lo, hi;
            if (!parseDateRange(std::string_view(lower).substr(colon + 1), now, lo, hi)) {
                fail("can't read date: " + value(colon + 1) + "  (try dm:today, dm:week, dm:2026-10-17)");
                continue;
            }
            q.minTime = std::max(q.minTime, lo);
            q.maxTime = std::min(q.maxTime, hi);
        } else {
            if (tok.find('/') != std::string_view::npos) q.matchPath = true;
            std::string term = q.matchCase ? std::string(tok) : std::move(lower);
            if (term.find_first_of("*?") != std::string::npos)
                (negate ? q.excludeGlobs : q.includeGlobs).push_back(std::move(term));
            else
                (negate ? q.exclude : q.include).push_back(std::move(term));
        }
    }
    return q;
}

}  // namespace fe
