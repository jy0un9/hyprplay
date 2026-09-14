# Changelog

## 0.1.0 — 2026-09-14

First public alpha aimed at Omarchy / Arch (AUR).

### Added

- Library support for MP3, M4A, and AAC alongside FLAC and Opus
- Import mode `copy` (default) and optional `convert_opus`
- Library enrichment pipeline (Discogs → title fix → lyrics) with resolve UI
- AppStream id `org.jy0un9.qt-music`, desktop MimeTypes, PKGBUILD
- README screenshot and contactable API User-Agents for Discogs / lyrics / MusicBrainz

### Changed

- Default library path is always `~/Music` (no personal `opusnew` probe)
- Config lives at `~/.config/qt-music/`; nested legacy path is migrated once
- UI font falls back when the configured family is missing
- Keyring schema renamed to `org.jy0un9.qt-music.Token`

### Notes

- GitHub homepage: https://github.com/jy0un9/qt-music
- Status: alpha — expect config/API churn before 0.2
