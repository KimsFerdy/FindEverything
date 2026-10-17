# macOS backend (Milestone 5)

Until this exists, macOS builds use `src/platform/generic/`.

- `MacScanner.cpp`: `getattrlistbulk()` returns names and metadata for many entries per syscall,
  which is much faster than `readdir` + `stat`.
- `FSEventsWatcher.cpp`: `FSEventStreamCreate` with `kFSEventStreamCreateFlagFileEvents`.
  The app needs Full Disk Access to see everything.
