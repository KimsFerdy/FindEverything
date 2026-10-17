# Windows backend (Milestone 5)

Until this exists, Windows builds use `src/platform/generic/`.

- `NtfsScanner.cpp`: open the volume (`\\.\C:`) and read the Master File Table with
  `FSCTL_ENUM_USN_DATA`. This is how Everything indexes a whole drive in seconds. Needs admin
  rights, or a small elevated service.
- `UsnWatcher.cpp`: follow changes with `FSCTL_READ_USN_JOURNAL`.
- Fallback for non-NTFS drives: the generic scanner plus `ReadDirectoryChangesW`.
