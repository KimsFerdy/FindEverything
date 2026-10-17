# Changelog

## 0.2.0-beta (unreleased)

### Live updates
- the index follows the disk while the app runs (inotify): new, renamed, moved and deleted files and
  whole folder trees show up within ~0.3 s, no rescan needed
- watches every folder within half the per-user inotify limit, home first; the status bar says when the limit
  is hit and how to raise it
- tombstones + compaction, changes are saved every minute and on quit
- Preferences > Indexes: "Keep the index up to date while running"

### Global shortcut
- press a shortcut anywhere to show / hide FindEverything (Preferences > General, off by default).
  registered as a Cinnamon / GNOME custom shortcut running `FindEverything --toggle`
- single instance: launching again (or `--search`) talks to the running app over D-Bus

### Search
- filter dropdown: Audio, Video, Pictures, Documents, Code, Archives, Executables
- wildcards `*.cpp`, `regex:`, `size:>100mb`, `dm:today` / `dm:week` / `dm:2026-10-17` / ranges, `path:`
- query errors (bad regex etc) show in the status bar instead of silently matching nothing

### Selection + Trash
- multi-select: click, ctrl+click, shift+click, shift+arrows, ctrl+a; survives sorting and live updates
- open / show in file manager / copy paths or names for the whole selection
- Move to Trash (Delete), always asks first. freedesktop trash spec, restorable from the file manager
- arrow keys move focus to the results (like Everything), typing goes back to the search box

### Other
- Help > Check for Updates (asks GitHub, only when you click it)
- lower memory use after scans (~110 MB less)

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
