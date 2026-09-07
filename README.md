# Qt Music

![Platform](https://img.shields.io/badge/platform-Linux-blue)
![Qt](https://img.shields.io/badge/Qt-6-41cd52)
![Audio](https://img.shields.io/badge/playback-libmpv-purple)
![License](https://img.shields.io/badge/license-MIT-blue)

A native Qt 6 desktop music player for local FLAC and Opus libraries on Linux — built for Omarchy/Hyprland with Wayland-first display detection, MPRIS2 integration, and synced LRC lyrics.

## Screenshot

![Qt Music library view](docs/assets/screenshot.png)

> Add a screenshot at `docs/assets/screenshot.png` (recommended: 1280×800 PNG of the library view with now-playing bar visible).

## Features

- **Library browser** — artist / album / track columns with fuzzy search (`/` to open, scope follows hover); optional filesystem watch for incremental rescans
- **libmpv playback** — queue, Fisher–Yates shuffle, repeat, gapless-friendly seeking, PipeWire/Pulse/ALSA output
- **Now playing** — waveform seek bar, album art, quality label (FLAC bit-depth / Opus bitrate), synced karaoke lyrics (`.lrc`)
- **Playlists** — M3U-style playlists beside your library; add album/track/multi-select; drag track numbers to reorder
- **Tag editing** — in-app editor for track, album, and artist tags via TagLib
- **Import inbox** — drop albums into an inbox, review per-album, ingest selected into the library
- **Discogs & beets** — fetch artist/album metadata from Discogs; optional beets CLI for autotagging
- **Secrets** — Discogs/Genius tokens prefer the system keyring (libsecret); fallback `secrets.toml` with mode `0600`
- **MPRIS2** — `org.mpris.MediaPlayer2.qt-music` with LoopStatus/Shuffle, Seek/SetPosition, OpenUri, and desktop media keys
- **Accessibility** — named roles on nav, lists, seek/volume, empty states, and menus for screen readers
- **Omarchy theme** — reads system accent/background colors and icon theme for a consistent desktop look
- **Layout persistence** — Browse/Playlists split-pane widths saved to config across restarts

## Requirements

Arch Linux packages (adjust equivalents for your distro):

```bash
sudo pacman -S qt6-base qt6-declarative mpv taglib ffmpeg libsecret
```

Optional:

- **beet** — beets CLI for import/autotag workflows
- **libxcb-cursor** — only if you fall back to X11 and see xcb-cursor errors

## Build

```bash
git clone https://origin.cursor.com/jy0un9/qt-music.git
cd qt-music
qmake6 qt-music.pro
make -j$(nproc)
./qt-music
```

Run from a terminal inside your Wayland session (Hyprland/Omarchy). The `./qt-music` launcher script discovers the Wayland socket under `$XDG_RUNTIME_DIR` when your shell did not inherit `WAYLAND_DISPLAY`.

## Tests

```bash
cd tests
qmake6 tests.pro
make -j$(nproc)
./qt-music-tests
```

CI (GitHub Actions) runs the unit suite plus an offscreen QML smoke:

```bash
QT_QPA_PLATFORM=offscreen timeout 5 ./qt-music-bin
```

## Install

**Primary install is `/usr/local`** (same binary the app launcher and PATH use on this machine):

```bash
qmake6 qt-music.pro
make -j$(nproc)
sudo make install
```

Then restart via the app menu or `/usr/local/bin/qt-music`.

Optional user install (no sudo) — use only if you are not also installing to `/usr/local`:

```bash
qmake6 qt-music.pro PREFIX=$HOME/.local
make -j$(nproc)
make install
```

Ensure `~/.local/bin` is on your `PATH`. If both prefixes are installed, the
launcher prints a warning; remove the user copy with:

```bash
qmake6 qt-music.pro PREFIX=$HOME/.local
make uninstall
```

Install does **not** touch library data, playlists, or config:

| Data | Location |
| --- | --- |
| Config / secrets | `~/.config/qt-music/` (`secrets.toml` mode `0600`; tokens prefer the system keyring when Secret Service is available) |
| Library DB | `~/.local/share/qt-music/` |
| Playlists / music / covers | Paths in `config.toml` (e.g. `~/Music/...`) |

Installed files:

| Path | Purpose |
| --- | --- |
| `PREFIX/lib/qt-music/qt-music-bin` | Application binary |
| `PREFIX/bin/qt-music` | Launcher (Wayland/Pulse auto-detect) |
| `PREFIX/share/applications/qt-music.desktop` | Desktop entry |
| `PREFIX/share/icons/hicolor/*/apps/qt-music.*` | App icon |

## Usage

| Key | Action |
| --- | --- |
| `/`, `Ctrl+F`, `Ctrl+K` | Open library search (Browse view) |
| `Tab` (in search) | Cycle search scope (artists → albums → tracks) |
| `Space` | Play / pause (when no text field is focused) |
| `↑` / `↓` | Move in Browse or Playlists columns |
| `←` / `→` | Column focus (→ plays highlighted track) |
| `PgUp` / `PgDn` | Seek backward / forward by step |
| `W` / `A` / `S` / `D` | Same as arrows (enable in Settings → Appearance) |
| `J` / `K` | Next / previous track |
| `Enter` | Play selection (Browse track / playlist / playlist track) |
| `Ctrl+click` / `Shift+click` | Multi-select tracks in Browse |
| `Ctrl+A` | Select all visible tracks (Browse) |
| `Delete` (playlists) | Delete playlist (left column) or remove track (right) |
| `Ctrl+N` | New playlist (focus name field) |
| `Ctrl+,` | Open settings |
| `?` | Keyboard shortcut help |
| `Esc` | Close dialog / library search / clear multi-select |

Use the top tabs to switch between **Browse**, **Playlists**, **Import**, and **Settings**. Right-click artists, albums, or tracks for tag editing, Discogs fetch, and **Add to playlist** (including the current multi-select). In Playlists, drag the track-number handle to reorder.

## Configuration

Config file: `~/.config/qt-music/qt-music/config.toml`

Library database: `~/.local/share/qt-music/library.db` (loaded on startup; rescans when empty, paths change, or you click **Rescan Library**).

Example:

```toml
[library]
paths = ["~/Music"]
scan_on_launch = true
library_watch = true   # auto-detect adds/removes under library paths
lyrics_dir = "~/Music/Lyrics"

[playlists]
dir = "~/Music/Playlists"

[playback]
volume = 80
seek_step_secs = 5
lyrics_offset_ms = 0   # positive = show lyrics earlier; negative = later

[layout]
library_artists_width = 220
library_albums_width = 240
side_panel_width = 300
now_playing_height = 108
playlists_list_width = 260

[beets]
binary = "beet"
nomove = true

[ui]
font_family = "JetBrainsMono Nerd Font"
```

Place sidecar `.lrc` files next to tracks or under `lyrics_dir` (flat or `Artist/Album/track.lrc`).

## Project structure

```
qt-music/
├── qt-music.pro          # qmake project file
├── qt-music              # launcher script (Wayland/Pulse detection)
├── tests/                # Qt Test unit suite (FuzzyMatch, LRC, M3U, lyrics parsers)
├── .github/workflows/    # CI: unit tests + offscreen QML smoke
├── desktop/              # .desktop entry and icons
├── resources/            # Qt resource bundle (QML, assets)
├── ROADMAP.md            # backlog and completed Should items
└── src/
    ├── main.cpp
    ├── core/             # services (library, playback, config, import, tags, …)
    ├── models/           # QML list models
    ├── mpris/            # MPRIS2 D-Bus player
    └── ui/qml/           # Main.qml, views, components
```

## Status

Active personal project — API and config keys may change between commits. See [ROADMAP.md](ROADMAP.md) for completed work and remaining Nice-to-haves. Bug reports and patches welcome.

## License

[MIT](LICENSE)
