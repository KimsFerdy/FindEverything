// no framework yet, CHECK + exit code is enough for now

#include <cstdio>
#include <ctime>
#include <filesystem>

#include "core/Index.h"
#include "core/IndexFile.h"
#include "core/Query.h"
#include "core/Search.h"
#include "core/Sort.h"

static int g_failures = 0;

#define CHECK(expr)                                                                       \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

static void testQueryParsing() {
    fe::Query q = fe::parseQuery("Foo  \"bar baz\" !TMP ext:.JPG;png folder:");
    CHECK(q.include.size() == 2);
    CHECK(q.include[0] == "foo");
    CHECK(q.include[1] == "bar baz");
    CHECK(q.exclude.size() == 1 && q.exclude[0] == "tmp");
    CHECK(q.extensions.size() == 2 && q.extensions[0] == "jpg" && q.extensions[1] == "png");
    CHECK(q.foldersOnly && !q.filesOnly);
    CHECK(fe::parseQuery("   ").include.empty());
}

static void testFullPath() {
    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId home = idx.add(root, "home", fe::kIsDir, 0, 0);
    fe::RecordId file = idx.add(home, "Notes.TXT", 0, 42, 0);
    CHECK(idx.fullPath(root) == "/");
    CHECK(idx.fullPath(file) == "/home/Notes.TXT");
    CHECK(idx.lowerName(file) == "notes.txt");
}

static void testSearch() {
    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId photos = idx.add(root, "Photos", fe::kIsDir, 0, 0);
    idx.add(photos, "beach.JPG", 0, 1, 0);
    idx.add(photos, "beach_tmp.jpg", 0, 1, 0);
    idx.add(photos, "beach.png", 0, 1, 0);
    idx.add(root, "readme", 0, 1, 0);

    CHECK(fe::search(idx, fe::parseQuery("beach")).size() == 3);
    CHECK(fe::search(idx, fe::parseQuery("beach !tmp")).size() == 2);
    CHECK(fe::search(idx, fe::parseQuery("ext:jpg")).size() == 2);
    CHECK(fe::search(idx, fe::parseQuery("folder:")).size() == 2);  // "/" and Photos
    CHECK(fe::search(idx, fe::parseQuery("")).size() == idx.size());
    // threaded and single threaded have to agree
    CHECK(fe::search(idx, fe::parseQuery("beach"), 4) == fe::search(idx, fe::parseQuery("beach"), 1));
}

static void testSearchOptions() {
    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId src = idx.add(root, "src", fe::kIsDir, 0, 0);
    fe::RecordId core = idx.add(src, "Core", fe::kIsDir, 0, 0);
    idx.add(core, "Index.cpp", 0, 1, 0);
    idx.add(core, "index_test.cpp", 0, 1, 0);
    idx.add(root, "reindexer.cpp", 0, 1, 0);
    fe::PathCache paths;
    paths.build(idx);
    CHECK(paths.dirPath(core) == "/src/Core");
    CHECK(paths.lowerDirPath(core) == "/src/core");

    auto count = [&](const char* text, fe::SearchOptions opts = {}) {
        return fe::search(idx, fe::parseQuery(text, opts), 1, &paths).size();
    };
    fe::SearchOptions matchCase;
    matchCase.matchCase = true;
    fe::SearchOptions wholeWord;
    wholeWord.wholeWord = true;
    fe::SearchOptions matchPath;
    matchPath.matchPath = true;
    fe::SearchOptions filesOnly;
    filesOnly.kind = fe::SearchOptions::Kind::Files;

    CHECK(count("index") == 3);
    CHECK(count("Index", matchCase) == 1);
    CHECK(count("index", wholeWord) == 2);  // "Index.cpp", "index_test.cpp" (not "reindexer")
    CHECK(count("core cpp") == 0);          // names only
    CHECK(count("core cpp", matchPath) == 2);
    CHECK(count("src/core") == 3);  // '/' turns on path matching: Core + its 2 files
    CHECK(count("src", filesOnly) == 0);
}

static void testIndexFile() {
    namespace fs = std::filesystem;
    const std::string path = (fs::temp_directory_path() / "fe_test_index.bin").string();

    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId home = idx.add(root, "Home", fe::kIsDir, 0, 111);
    idx.add(home, "Notes.TXT", 0, 42, 222);
    CHECK(fe::saveIndex(idx, path, "/\n"));

    fe::Index loaded;
    fe::IndexFileInfo info;
    CHECK(fe::loadIndex(loaded, path, &info));
    CHECK(info.source == "/\n");
    CHECK(info.createdAt > 0);
    CHECK(loaded.size() == 3);
    CHECK(loaded.fullPath(2) == "/Home/Notes.TXT");
    CHECK(loaded.lowerName(2) == "notes.txt");
    CHECK(loaded.record(2).size == 42 && loaded.record(2).mtime == 222);

    // truncated file has to be rejected, not half loaded
    fs::resize_file(path, fs::file_size(path) - 1);
    CHECK(!fe::loadIndex(loaded, path));
    CHECK(loaded.size() == 0);
    CHECK(!fe::loadIndex(loaded, path + ".missing"));
    fs::remove(path);
}

static void testSort() {
    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId b = idx.add(root, "b", fe::kIsDir, 0, 50);
    fe::RecordId zip = idx.add(b, "a.zip", 0, 300, 10);
    fe::RecordId apple = idx.add(root, "Apple.txt", 0, 100, 30);
    fe::RecordId cat = idx.add(root, "cat.txt", 0, 200, 20);
    fe::PathCache paths;
    paths.build(idx);
    fe::SortOrders sorts;
    sorts.build(idx, paths);

    using V = std::vector<fe::RecordId>;
    const V all = {root, b, zip, apple, cat};
    auto sorted = [&](const V& r, fe::SortKey key, bool desc = false) {
        return fe::sortResults(r, idx.size(), sorts, key, desc);
    };
    // folders first, then a..z ignoring case
    CHECK((sorted(all, fe::SortKey::Name) == V{root, b, zip, apple, cat}));
    CHECK((sorted({cat, apple}, fe::SortKey::Name) == V{apple, cat}));
    CHECK((sorted({cat, apple}, fe::SortKey::Name, true) == V{cat, apple}));
    CHECK((sorted({zip, apple, cat}, fe::SortKey::Size, true) == V{zip, cat, apple}));
    CHECK((sorted({zip, apple, cat}, fe::SortKey::Modified) == V{zip, cat, apple}));
    // "/" (apple, cat) before "/b" (zip)
    CHECK((sorted({zip, cat, apple}, fe::SortKey::Path) == V{apple, cat, zip}));
}

static void testLiveMutations() {
    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId a = idx.add(root, "a", fe::kIsDir, 0, 0);
    fe::RecordId b = idx.add(root, "b", fe::kIsDir, 0, 0);
    fe::RecordId note = idx.add(a, "note.txt", 0, 5, 1);
    fe::RecordId sub = idx.add(a, "sub", fe::kIsDir, 0, 0);
    idx.add(sub, "deep.txt", 0, 1, 1);

    // move a under a folder that comes later in the index + rename it
    fe::RecordId later = idx.add(b, "later", fe::kIsDir, 0, 0);
    idx.move(a, later, "renamed");
    CHECK(idx.fullPath(note) == "/b/later/renamed/note.txt");
    CHECK(fe::search(idx, fe::parseQuery("renamed")).size() == 1);
    CHECK(fe::search(idx, fe::parseQuery("b/later/renamed/sub"), 1).size() == 0);  // no PathCache = names only

    idx.setMeta(note, 0, 99, 7);
    CHECK(idx.record(note).size == 99 && idx.record(note).mtime == 7);

    // tombstones disappear from search
    idx.markDeleted(note);
    CHECK(fe::search(idx, fe::parseQuery("note")).empty());

    // compaction: tombstones and their subtrees gone, parents first again, paths unchanged
    idx.markDeleted(sub);
    fe::Index c = idx.compacted();
    CHECK(c.size() == 4);  // "/", b, later, renamed. note + sub (and deep.txt under it) are gone
    for (fe::RecordId id = 0; id < c.size(); ++id) {
        CHECK(c.record(id).parent == fe::kNoParent || c.record(id).parent < id);
        CHECK(!c.isDeleted(id));
    }
    CHECK(fe::search(c, fe::parseQuery("deep")).empty());
    bool foundRenamed = false;
    for (fe::RecordId id = 0; id < c.size(); ++id) foundRenamed |= c.fullPath(id) == "/b/later/renamed";
    CHECK(foundRenamed);
}

static void testSyntax() {
    CHECK(fe::globMatch("*.cpp", "main.cpp"));
    CHECK(!fe::globMatch("*.cpp", "main.cpp.bak"));
    CHECK(fe::globMatch("report_??.pdf", "report_07.pdf"));
    CHECK(!fe::globMatch("report_??.pdf", "report_7.pdf"));
    CHECK(fe::globMatch("a*b*c", "aXXbYYc"));
    CHECK(fe::globMatch("*", ""));

    // 2026-10-17 12:00 local, so "today" etc are stable
    std::tm tm{};
    tm.tm_year = 126, tm.tm_mon = 9, tm.tm_mday = 17, tm.tm_hour = 12, tm.tm_isdst = -1;
    const int64_t now = std::mktime(&tm);
    auto at = [](int y, int m, int d, int h) {
        std::tm t{};
        t.tm_year = y - 1900, t.tm_mon = m - 1, t.tm_mday = d, t.tm_hour = h, t.tm_isdst = -1;
        return static_cast<int64_t>(std::mktime(&t));
    };

    fe::Index idx;
    fe::RecordId root = idx.add(fe::kNoParent, "/", fe::kIsDir, 0, 0);
    fe::RecordId music = idx.add(root, "music", fe::kIsDir, 0, at(2026, 10, 17, 9));
    idx.add(music, "song.mp3", 0, 5 << 20, at(2026, 10, 17, 9));            // 5 MB, today
    idx.add(music, "cover.JPG", 0, 300 << 10, at(2026, 10, 16, 20));        // 300 KB, yesterday
    idx.add(root, "main.cpp", 0, 2 << 10, at(2026, 9, 1, 10));              // 2 KB, last month
    idx.add(root, "main.cpp.bak", 0, 2 << 10, at(2025, 1, 1, 10));          // last year
    idx.add(root, "IMG_2041.png", 0, 1 << 30, at(2026, 10, 10, 10));        // 1 GB
    idx.add(root, "run-me", fe::kIsExecutable, 100, at(2026, 10, 17, 11));  // exec bit, no extension
    fe::PathCache paths;
    paths.build(idx);

    auto count = [&](const char* text, fe::Filter filter = fe::Filter::All) {
        fe::SearchOptions opts;
        opts.filter = filter;
        return fe::search(idx, fe::parseQuery(text, opts, now), 1, &paths).size();
    };
    CHECK(count("*.cpp") == 1);
    CHECK(count("!*.bak main") == 1);
    CHECK(count("regex:^img_[0-9]{4}") == 1);
    CHECK(count("regex:^IMG", fe::Filter::All) == 1);  // case-insensitive by default
    CHECK(count("!regex:\\.(cpp|bak)$ file:") == 4);
    CHECK(count("size:>1mb") == 2);
    CHECK(count("size:<4kb") == 3);  // main.cpp, .bak, run-me (folders never match size:)
    CHECK(count("size:100kb..1mb") == 1);
    CHECK(count("dm:today") == 3);  // music folder, song, run-me
    CHECK(count("dm:yesterday") == 1);
    CHECK(count("dm:week file:") == 3);  // last 7 days = after oct 10 12:00, IMG (10:00 that day) is just out
    CHECK(count("dm:2026-10-16") == 1);
    CHECK(count("dm:>2026-10-16 file:") == 2);
    CHECK(count("dm:2026-01-01..2026-10-10 file:") == 2);  // main.cpp + IMG (on the 10th, end day included)
    CHECK(count("", fe::Filter::Audio) == 1);
    CHECK(count("", fe::Filter::Pictures) == 2);
    CHECK(count("", fe::Filter::Code) == 1);
    CHECK(count("", fe::Filter::Executables) == 1);
    CHECK(count("cover", fe::Filter::Audio) == 0);
    CHECK(count("path: music cover") == 1);

    CHECK(fe::parseQuery("regex:([", {}, now).error.find("invalid regex") == 0);
    CHECK(fe::parseQuery("size:>lots", {}, now).error.find("size") != std::string::npos);
    CHECK(fe::parseQuery("dm:someday", {}, now).error.find("date") != std::string::npos);
    CHECK(fe::parseQuery("regex:([ song", {}, now).include.size() == 1);  // bad term skipped, rest still works
}

int main() {
    testSyntax();
    testLiveMutations();
    testSort();
    testQueryParsing();
    testFullPath();
    testSearch();
    testSearchOptions();
    testIndexFile();
    if (g_failures == 0) std::puts("all tests passed");
    return g_failures == 0 ? 0 : 1;
}
