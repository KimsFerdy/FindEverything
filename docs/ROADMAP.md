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
- [ ] Double-click opens the file (`xdg-open` via `fork` + `execvp`)
- [ ] Right-click → "Open containing folder", "Copy path"
- [ ] Click a column header to sort (`ImGuiTableFlags_Sortable`)
- [ ] Ctrl+F / Esc keyboard shortcuts, remember window size

## M3: Live updates on Linux
- [ ] `Index` supports delete and rename (tombstones + periodic compaction)
- [ ] Path → RecordId lookup (hash map) so events can find their record
- [ ] `LinuxWatcher` with fanotify (`FAN_MARK_FILESYSTEM`, `FAN_REPORT_DFID_NAME`)
- [ ] inotify fallback when not running as root
- [ ] Optional: install a small root helper (systemd service) so the GUI can run as a normal user

## M4: Fast startup
- [ ] Save the index to `~/.cache/findeverything/` on exit; `mmap` it on start
- [ ] Rescan in the background and apply the differences
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
