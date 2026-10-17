# Changelog

## 0.1.0 — 2026-10-17

First release.

### Search
- in-memory index of every file and folder, multi-threaded search as you type
- Everything-style syntax: `foo bar`, `"phrase"`, `!not`, `ext:jpg;png`, `file:`, `folder:`, paths with `/`
- match case, whole word, match path, files / folders only
- sort by name, path, size or date (pre-sorted, instant even on 500k results)

### Indexing
- fast posix scanner, every disk scanned as its own root, virtual filesystems skipped
- saved index in `~/.cache/findeverything` so it starts instantly, rescan in the background
- pick which disks to index, exclude folders

### Desktop
- system font, dark / light, accent color and file icons from your desktop theme
- open, show in file manager, copy path / name, right-click menu, keyboard navigation
- tray icon (StatusNotifierItem), close to tray, start in tray at login
- remembers window size / position and column layout
- app icon, `.desktop` file, `cmake --install`
- packages: `.deb`, AppImage, `.tar.gz`

### CLI
- `fe-cli` shares the gui's settings and saved index
- `FindEverything --search "text"` opens with a query

### Known limitations
- no live updates yet, new files show up after a rescan (startup or `F5`)
- file names in scripts other than latin / greek / cyrillic show as `?`
- X11 build of GLFW; on GNOME the tray needs the AppIndicator extension
