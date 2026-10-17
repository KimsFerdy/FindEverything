# Roadmap

Each milestone ends with something you can run.

## M1: Index + CLI ✅ (scaffolded)
- [x] `Index`: flat records + name pool
- [x] Linux scanner (`open` / `readdir` / `fstatat`), skips virtual filesystems via mountinfo
- [x] Query parser: AND terms, phrases, `!`, `ext:`, `file:`, `folder:`
- [x] Multithreaded search
- [x] `fe-cli` with timings
- [ ] Benchmark on `/` and record the numbers here

## M2: GUI ✅ (scaffolded)
- [x] ImGui window, search-as-you-type, virtualised results table
- [x] Background indexing with a live counter
- [x] Double-click / Enter opens the file (`xdg-open` via `fork` + `execvp`)
- [x] Right-click → Open, Open containing folder (FileManager1 D-Bus), Copy path / name
- [x] Keyboard: ↑↓ PgUp PgDn, Enter, Ctrl+Enter, Ctrl+Shift+C, Ctrl+F / Esc
- [x] System font (gsettings + fontconfig), dark/light + accent colour (XDG portal)
- [x] Menu bar: File / Edit / View / Search / Help, rescan (F5), zoom, column toggles
- [x] Match case, whole word, match path (folder path cache), files/folders only
- [ ] Multi-select (Ctrl/Shift+click) + Select All
- [ ] Click a column header to sort (`ImGuiTableFlags_Sortable`)
- [ ] Remember window size and column widths
- [ ] Follow theme changes live (portal `SettingChanged` signal)
- [ ] Non-Latin file names: upgrade to ImGui 1.92 dynamic fonts + fallback fonts

## M3: Live updates on Linux
- [ ] `Index` supports delete and rename (tombstones + periodic compaction)
- [ ] Path → RecordId lookup (hash map) so events can find their record
- [ ] `LinuxWatcher` with fanotify (`FAN_MARK_FILESYSTEM`, `FAN_REPORT_DFID_NAME`)
- [ ] inotify fallback when not running as root
- [ ] Optional: install a small root helper (systemd service) so the GUI can run as a normal user

## M4: Fast startup
- [x] Save the index to `~/.cache/findeverything/index.bin` after each scan (atomic rename)
- [x] Load it at startup (~0.2 s for 500k entries), then rescan in the background
- [x] fe-cli shares the cache (`--rescan` to refresh)
- [ ] `mmap` the file instead of reading it (zero-copy, even faster start)
- [ ] Apply only the differences instead of swapping the whole index
- [ ] Settings file (`~/.config/findeverything/config.toml`): roots, excludes

## M5: Other platforms
- [ ] Windows: NTFS MFT scanner + USN journal watcher
- [ ] macOS: `getattrlistbulk` scanner + FSEvents
- [ ] GitHub Actions CI building all three platforms

## M6: Polish
- [ ] Full UTF-8 case-insensitive matching
- [ ] Wildcards and regex
- [ ] Global hotkey to show the window
- [ ] Packaging: AppImage / .deb, Windows installer, macOS .app
