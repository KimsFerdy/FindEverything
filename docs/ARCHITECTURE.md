# Architecture

```
src/
  core/       index, query parser, search, sort, index file. plain c++, no os calls
  platform/   everything linux: scanner, mounts, xdg paths, mime + icon themes, autostart, xdg-open
  app/        imgui gui, settings, preferences
  cli/        fe-cli
tests/        core tests
```

## How it's fast

Same tricks as Everything:

1. **Everything lives in RAM.** One flat array of 32-byte records plus one big string with all the names.
   Searching is a linear scan over memory, split across all cores.
2. **Names, not paths.** Each record stores its name and its parent's id. Full paths only get built
   for the rows on screen. Folder paths are cached separately (`PathCache`) for path search.
3. **Pre-sorted columns.** The whole index is sorted by name/path/size/date once after each scan
   (`SortOrders`). Sorting results is a bitmap filter over one of those lists, ~3 ms for 500k.
4. **Only visible rows get drawn** (`ImGuiListClipper`).
5. **Saved index.** Dumped to `~/.cache/findeverything/index.bin` after every scan and loaded on
   startup, so results show up before the rescan finishes.

## Threads

| thread       | does                                                                 |
|--------------|----------------------------------------------------------------------|
| ui           | owns the current index, draws, searches                              |
| scan         | loads the cache / scans / saves, builds paths + sort orders, hands over an `IndexSnapshot` |
| search pool  | short lived, one slice of the index each                             |

## Scanning

`open(O_DIRECTORY)` + `readdir` + `fstatat`, iterative DFS. Every real mount from
`/proc/self/mountinfo` is its own root, and the scanner stops when `st_dev` changes so disks
don't get walked twice. Virtual filesystems (`/proc`, `/sys`, tmpfs, snaps...) are skipped.

## Icons

file name → MIME type via shared-mime-info (`/usr/share/mime/globs2`) → icon name → file via the
freedesktop icon theme spec (with inheritance, e.g. Mint-Y-Aqua → Mint-Y → Adwaita → hicolor).
PNG/SVG decoded once per type into a GL texture.
