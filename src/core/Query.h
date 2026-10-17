#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fe {

// toggles from the Search menu
struct SearchOptions {
    enum class Kind { All, Files, Folders };
    bool matchCase = false;
    bool wholeWord = false;
    bool matchPath = false;
    Kind kind = Kind::All;
};

// small subset of Everything's syntax:
//   foo bar        both terms (AND)
//   "foo bar"      phrase
//   !tmp           not
//   ext:jpg;png    extension
//   file: folder:  only files / folders
//   src/main       a '/' in a term turns on path matching
struct Query {
    std::vector<std::string> include;     // lowercased unless matchCase
    std::vector<std::string> exclude;     // same
    std::vector<std::string> extensions;  // always lowercase, no dot
    bool filesOnly = false;
    bool foldersOnly = false;
    bool matchCase = false;
    bool wholeWord = false;
    bool matchPath = false;

    bool empty() const {
        return include.empty() && exclude.empty() && extensions.empty() && !filesOnly && !foldersOnly;
    }
};

Query parseQuery(std::string_view text, const SearchOptions& options = {});

}  // namespace fe
