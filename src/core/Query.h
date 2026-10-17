#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fe {

// A parsed search string. Syntax (a small subset of Everything's):
//
//   foo bar          name contains "foo" AND "bar"   (case-insensitive)
//   "foo bar"        name contains the phrase "foo bar"
//   !tmp             name does NOT contain "tmp"
//   ext:jpg;png      extension is jpg or png
//   file:  folder:   only files / only folders
struct Query {
    std::vector<std::string> include;     // lowercased
    std::vector<std::string> exclude;     // lowercased
    std::vector<std::string> extensions;  // lowercased, without the dot
    bool filesOnly = false;
    bool foldersOnly = false;

    bool empty() const {
        return include.empty() && exclude.empty() && extensions.empty() && !filesOnly && !foldersOnly;
    }
};

Query parseQuery(std::string_view text);

}  // namespace fe
