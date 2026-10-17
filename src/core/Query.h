#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace fe {

// the filter dropdown next to the search box
enum class Filter { All, Audio, Video, Pictures, Documents, Code, Archives, Executables, Count };
const char* filterName(Filter filter);

// toggles from the Search menu
struct SearchOptions {
    enum class Kind { All, Files, Folders };
    bool matchCase = false;
    bool wholeWord = false;
    bool matchPath = false;
    Kind kind = Kind::All;
    Filter filter = Filter::All;
};

// Everything-style syntax:
//   foo bar             both terms (AND)
//   "foo bar"           phrase
//   !tmp                not (works for wildcards and regex too)
//   ext:jpg;png         extension
//   *.cpp  report_??    wildcards, match the whole name (or the whole path with path matching)
//   regex:^img_\d+      posix extended regex
//   size:>100mb  size:1mb..1gb  size:<4kb
//   dm:today  dm:week  dm:2026-10-17  dm:>2026-01-01  dm:2026-01-01..2026-02-01
//   file: folder:       only files / folders
//   path:               match against full paths, a '/' in a term does that too
struct Query {
    std::vector<std::string> include;  // lowercased unless matchCase
    std::vector<std::string> exclude;
    std::vector<std::string> includeGlobs;  // contain * or ?
    std::vector<std::string> excludeGlobs;
    std::vector<std::string> includeRegex;  // validated already, see error
    std::vector<std::string> excludeRegex;
    std::vector<std::string> extensions;        // ext:, always lowercase, no dot. any of them
    std::vector<std::string> filterExtensions;  // from the filter dropdown. any of them
    bool filterExecutables = false;
    uint64_t minSize = 0, maxSize = std::numeric_limits<uint64_t>::max();
    int64_t minTime = std::numeric_limits<int64_t>::min(), maxTime = std::numeric_limits<int64_t>::max();  // [min, max)
    bool filesOnly = false;
    bool foldersOnly = false;
    bool matchCase = false;
    bool wholeWord = false;
    bool matchPath = false;
    std::string error;  // bad regex / size / date, shown in the status bar. the bad term is ignored

    bool hasSize() const { return minSize != 0 || maxSize != std::numeric_limits<uint64_t>::max(); }
    bool hasTime() const {
        return minTime != std::numeric_limits<int64_t>::min() || maxTime != std::numeric_limits<int64_t>::max();
    }
};

// now = unix time used for dm:today etc, 0 = the real clock (tests pass a fixed one)
Query parseQuery(std::string_view text, const SearchOptions& options = {}, int64_t now = 0);

// '*' and '?' only, whole string
bool globMatch(std::string_view pattern, std::string_view text);

}  // namespace fe
