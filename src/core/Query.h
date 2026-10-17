#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fe {

// Toggles that live outside the search text (the Search menu).
struct SearchOptions {
    enum class Kind { All, Files, Folders };
    bool matchCase = false;
    bool wholeWord = false;
    bool matchPath = false;  // match against the full path, not just the name
    Kind kind = Kind::All;
};

// A parsed search string. Syntax (a small subset of Everything's):
//
//   foo bar          name contains "foo" AND "bar"   (case-insensitive by default)
//   "foo bar"        name contains the phrase "foo bar"
//   !tmp             name does NOT contain "tmp"
//   ext:jpg;png      extension is jpg or png
//   file:  folder:   only files / only folders
//   src/main         a term containing '/' or '\' switches on path matching
struct Query {
    std::vector<std::string> include;     // lowercased unless matchCase
    std::vector<std::string> exclude;     // lowercased unless matchCase
    std::vector<std::string> extensions;  // always lowercased, without the dot
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
