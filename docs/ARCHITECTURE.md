# Architecture

```
   ┌──────────────┐   ┌──────────────┐
   │  app (GUI)   │   │  cli         │      front-ends: only talk to core + platform headers
   └──────┬───────┘   └──────┬───────┘
          └────────┬─────────┘
          ┌────────▼─────────┐
          │ platform         │  Scanner.h / Watcher.h: same interface, one .cpp per OS,
          │ linux | generic  │  CMake picks which file gets compiled
          └────────┬─────────┘
          ┌────────▼─────────┐
          │ core             │  Index, Query, Search: pure C++, compiles anywhere
          └──────────────────┘
```

## Why Everything is fast, and how we copy it

1. **It never searches the disk.** Everything reads the whole file list once into RAM. A search
   is just a loop over memory, and scanning a few hundred MB of names takes milliseconds.
   → `core/Index`
2. **It stores names, not paths.** Each record holds its own name and a parent id. Full paths are
   rebuilt only for the ~30 rows on screen. This saves a lot of memory.
   → `FileRecord::parent`, `Index::fullPath`
3. **It doesn't crawl NTFS.** It reads the Master File Table directly, then follows the USN change
   journal. Linux has no MFT, so we crawl once and then use **fanotify** for changes.
   → `platform/linux`, `platform/windows/README.md`
4. **It searches in parallel.** Names are split into one chunk per CPU core.
   → `core/Search.cpp`
5. **It only draws visible rows.** → `ImGuiListClipper` in `app/MainWindow.cpp`

## Threading

| Thread       | Owns                       | Talks via                                   |
|--------------|----------------------------|---------------------------------------------|
| UI (main)    | `MainWindow::index_`       | -                                           |
| Scanner      | a fresh `Index` while building | hands it to UI through `pending_` + mutex |
| Search pool  | short-lived per search     | each writes to its own vector, no locks     |
| Watcher (M3) | event queue                | UI applies events between frames            |

## Ideas for later

- **Persisting the index** (`~/.cache/findeverything/index.bin`): write records and the name
  pool straight to disk, then `mmap` them on startup so launch is instant.
- **Sorting** by name, path, size or date: keep pre-sorted id arrays, like Everything does.
- **Path search**: when the query contains `/`, match against the full path, not only the name.
- **Regex / wildcards** (`*.c`, `regex:`).
