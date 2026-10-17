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
| ui           | owns a read-only snapshot, draws, searches                           |
| index worker | `IndexService`: loads the cache, scans, owns the real index, applies inotify events, saves |
| builder      | turns copies of the real index into `IndexSnapshot`s (path cache + sort orders), newest wins |
| search pool  | short lived, one slice of the index each                             |

## Scanning

`open(O_DIRECTORY)` + `readdir` + `fstatat`, iterative DFS. Every real mount from
`/proc/self/mountinfo` is its own root, and the scanner stops when `st_dev` changes so disks
don't get walked twice. Virtual filesystems (`/proc`, `/sys`, tmpfs, snaps...) are skipped.

## Live updates

`IndexService` puts an inotify watch on every indexed folder (home first, at most half of
`max_user_watches` since every app of the user shares it). Events map back to records through
watch descriptor → folder record and a children table (csr + a small overflow map for new entries).

- create: stat + append; a new folder gets a watch first, then its contents get scanned (`scanInto`)
- delete: tombstone the record and its subtree
- rename / move: `IN_MOVED_FROM` + `IN_MOVED_TO` with the same cookie → change parent + name in place,
  so subtrees and their watches just follow. a lone `MOVED_FROM` (moved somewhere unwatched) becomes a delete
- queue overflow: full rescan

Changes get published ~300 ms after things go quiet (2 s max during a storm). Saving compacts first:
drops tombstones and puts parents before children again, which the index file requires.

## Icons

file name → MIME type via shared-mime-info (`/usr/share/mime/globs2`) → icon name → file via the
freedesktop icon theme spec (with inheritance, e.g. Mint-Y-Aqua → Mint-Y → Adwaita → hicolor).
PNG/SVG decoded once per type into a GL texture.
