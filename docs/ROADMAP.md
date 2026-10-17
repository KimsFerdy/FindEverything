# Roadmap

## Done
- [x] in-memory index, multi-threaded search, Everything-style query syntax
- [x] linux scanner (posix, one root per mount, skips virtual filesystems)
- [x] gui: search as you type, virtualized table, keyboard navigation
- [x] open / show in file manager / copy path, right-click menu
- [x] system look: font, dark/light, accent (xdg portal), icon theme
- [x] menu bar, match case / whole word / path, files / folders only, zoom
- [x] saved index for instant startup, background rescan
- [x] sort by name / path / size / date (pre-sorted orders)
- [x] preferences: start at login, rescan on startup, disks, excluded folders, theme, accent, font size
- [x] fe-cli sharing the gui's index
- [x] tray icon (StatusNotifierItem + dbusmenu over sd-bus), close to tray, start in tray at login
- [x] app icon, .desktop file, `cmake --install`
- [x] .deb / AppImage / .tar.gz (`scripts/package.sh`)

## 0.2 beta
- [x] live updates with inotify: create / delete / rename / move incl. whole trees, tombstones + compaction
- [x] snapshots built off-thread, saved every minute and on quit
- [x] help > check for updates
- [x] global shortcut (cinnamon / gnome custom keybinding) + single instance over d-bus
- [x] filters, wildcards, regex:, size:, dm:, path:
- [x] multi-select + bulk actions + move to trash (freedesktop trash spec)
- [ ] incremental sort orders (today every live change rebuilds them, ~0.5 s of cpu on 500k files)
- [ ] fanotify when running as root / with a helper (`FAN_MARK_FILESYSTEM`, no per-folder watches)
- [ ] notice changes made while the app wasn't running without a full rescan (compare dir mtimes)

## 0.3 ideas
- [ ] "Search with FindEverything" in the file manager's right-click menu (folder or empty space):
      opens FindEverything scoped to that folder (`--search "<folder>/ "`). nemo action file in
      ~/.local/share/nemo/actions, nautilus script, dolphin service menu; toggle in Preferences
- [ ] quick preview pane (space: image thumbnail / start of a text file / folder size)
- [ ] recently opened + rank often-opened files higher
- [ ] big files & duplicates view
- [ ] drag results out to other apps (xdnd)
- [ ] search inside files (`content:`)

## Later
- [ ] persist sort orders in the cache (skip the ~0.5 s rebuild on startup)
- [ ] `mmap` the index file
- [ ] natural sort ("file2" before "file10")
- [ ] remember window size / column widths
- [ ] follow theme changes live (portal `SettingChanged`)
- [ ] non-latin file names: imgui 1.92 dynamic fonts + fallback fonts
- [ ] proper utf-8 case-insensitive matching
- [ ] content sniffing for extensionless files (`mime/magic`)
- [ ] notice newly plugged in drives (poll `/proc/self/mountinfo`)
- [ ] wildcards + regex
- [ ] global hotkey
- [ ] wayland build of glfw
- [ ] flatpak, arm64 builds, CI that builds the release packages
