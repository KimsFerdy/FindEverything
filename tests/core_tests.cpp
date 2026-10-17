// Minimal dependency-free tests. Swap in Catch2 or GoogleTest once this grows.

#include "core/Index.h"
#include "core/Query.h"
#include "core/Search.h"

#include <cstdio>

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

int main() {
    testQueryParsing();
    testFullPath();
    testSearch();
    if (g_failures == 0) std::puts("all tests passed");
    return g_failures == 0 ? 0 : 1;
}
