# FindEverything

Instant file-name search for Linux, then Windows and macOS. Inspired by
[Everything by voidtools](https://www.voidtools.com/).

Stack: **C++20 · CMake · Dear ImGui · GLFW · OpenGL**

## Quick start (Linux Mint / Ubuntu)

```bash
./scripts/install-deps-linux.sh          # one time: compiler, cmake, ninja, X11/GL headers

cmake --preset release                   # configure (downloads ImGui + GLFW the first time)
cmake --build --preset release           # build

./build/release/FindEverything           # GUI
./build/release/fe-cli --root ~ ext:pdf  # terminal version, good for testing and benchmarks
ctest --preset debug                     # tests (after: cmake --preset debug && cmake --build --preset debug)
```

To search folders you can't normally read (other users' homes, `/root`), run it with `sudo`.
Otherwise those folders are skipped and counted as "unreadable dirs".

## Search syntax

| Query          | Meaning                                |
|----------------|----------------------------------------|
| `foo bar`      | name contains `foo` **and** `bar`      |
| `"foo bar"`    | exact phrase                           |
| `!tmp`         | name does not contain `tmp`            |
| `ext:jpg;png`  | extension is jpg or png                |
| `file:` / `folder:` | only files / only folders         |

## Project layout

```
FindEverything/
├── CMakeLists.txt            build definition: fe_core → fe_platform → apps
├── CMakePresets.json         debug / release / cli-only presets
├── cmake/Dependencies.cmake  fetches ImGui + GLFW
├── docs/
│   ├── ARCHITECTURE.md       how the pieces fit together
│   ├── ROADMAP.md            milestones, in build order
│   └── LINUX_LEARNING.md     Linux concepts this project teaches, mapped to code
├── scripts/install-deps-linux.sh
├── src/
│   ├── core/                 NO OS code: index, query parser, search
│   ├── platform/
│   │   ├── Scanner.h         "walk a filesystem into an Index" (one impl per OS)
│   │   ├── Watcher.h         "tell me when files change" (one impl per OS)
│   │   ├── linux/            POSIX scanner, /proc/self/mountinfo, fanotify (todo)
│   │   ├── generic/          std::filesystem fallback (Windows/macOS for now)
│   │   ├── windows/          NTFS MFT + USN journal (todo)
│   │   └── macos/            getattrlistbulk + FSEvents (todo)
│   ├── app/                  ImGui GUI
│   └── cli/                  fe-cli
└── tests/                    core unit tests
```
