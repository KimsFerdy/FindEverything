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

## Next: live updates
- [ ] index supports delete / rename (tombstones + compaction)
- [ ] path → record lookup so events can find their entry
- [ ] fanotify watcher (`FAN_MARK_FILESYSTEM` + `FAN_REPORT_DFID_NAME`, needs `CAP_SYS_ADMIN`)
- [ ] inotify fallback when running unprivileged
- [ ] maybe a small root helper service so the gui can stay a normal user

## Later
- [ ] persist sort orders in the cache (skip the ~0.5 s rebuild on startup)
- [ ] `mmap` the index file
- [ ] natural sort ("file2" before "file10")
- [ ] multi-select + select all
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
