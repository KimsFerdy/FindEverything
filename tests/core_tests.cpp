// Minimal dependency-free tests. Swap in Catch2 or GoogleTest once this grows.

#include "core/Index.h"
#include "core/IndexFile.h"
#include "core/Query.h"
#include "core/Search.h"

#include <cstdio>
#include <filesystem>

static int g_failures = 0;

#define CHECK(expr)                                                         \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #expr); \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

static void testQueryParsing() {
    fe::Query q = fe::parseQuery("Foo  \"bar baz\" !TMP ext:.JPG;png folder:");
    CHECK(q.include.size() == 2);
    CHECK(q.include[0] == "foo");
    CHECK(q.include[1] == "bar baz");
    CHECK(q.exclude.size() == 1 && q.exclude[0] == "tmp");
    CHECK(q.extensions.size() == 2 && q.extensions[0] == "jpg" && q.extensions[1] == "png");
    CHECK(q.foldersOnly && !q.filesOnly);
    CHECK(fe::parseQuery("   ").empty());
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
    // Multi-threaded path must give identical results.
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
    CHECK(count("index", wholeWord) == 2);    // "Index.cpp", "index_test.cpp" (not "reindexer")
    CHECK(count("core cpp") == 0);            // names only
    CHECK(count("core cpp", matchPath) == 2);
    CHECK(count("src/core") == 3);            // '/' turns on path matching: Core + its 2 files
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

    // A truncated file must be rejected, not half-loaded.
    fs::resize_file(path, fs::file_size(path) - 1);
    CHECK(!fe::loadIndex(loaded, path));
    CHECK(loaded.size() == 0);
    CHECK(!fe::loadIndex(loaded, path + ".missing"));
    fs::remove(path);
}

int main() {
    testQueryParsing();
    testFullPath();
    testSearch();
    testSearchOptions();
    testIndexFile();
    if (g_failures == 0) std::puts("all tests passed");
    return g_failures == 0 ? 0 : 1;
}
