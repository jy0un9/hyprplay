# Changelog

## 0.1.0 — 2026-09-14

First alpha as **Hyprplay** (renamed from the earlier qt-music codename).

### Added

- Library support for MP3, M4A, and AAC alongside FLAC and Opus
- Import mode `copy` (default) and optional `convert_opus`
- Library enrichment pipeline (Discogs → title fix → lyrics) with resolve UI
- AppStream id `org.jy0un9.hyprplay`, desktop MimeTypes, PKGBUILD
- Equalizer-bar app icon and Hyprplay branding

### Changed

- Full rebrand to Hyprplay (binary, desktop entry, MPRIS, config paths)
- Migrates legacy `~/.config/qt-music/` and library DB on first launch
- Default library path is always `~/Music`
- UI font falls back when the configured family is missing

### Notes

- GitHub: https://github.com/jy0un9/hyprplay (private for now)
- Status: alpha — expect config/API churn before 0.2
