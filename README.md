# Hyprplay

![Platform](https://img.shields.io/badge/platform-Linux-blue)
![Qt](https://img.shields.io/badge/Qt-6-41cd52)
![License](https://img.shields.io/badge/license-MIT-blue)

Local music player for Linux — built for Omarchy/Hyprland. Browse your library,
play with libmpv, manage playlists, edit tags, and fetch synced lyrics.

## Screenshots

![Browse](docs/assets/browse.png)

![Playlists](docs/assets/playlists.png)

![Settings](docs/assets/settings.png)

## Features

- Browse by artist → album → track, with search (`/`)
- Plays FLAC, Opus, Ogg Vorbis, MP3, M4A, and AAC
- Waveform seek bar, karaoke lyrics, and Omarchy theme sync
- Playlists, tag editing, and optional Discogs library enrichment
- Desktop media keys via MPRIS2; open audio files from the file manager

## Install

**Dependencies (Arch / Omarchy):**

```bash
sudo pacman -S qt6-base qt6-declarative mpv taglib ffmpeg libsecret
```

Optional: `beets` (import tagging), `opus-tools` (FLAC→Opus convert).

**Build and install:**

```bash
git clone https://github.com/jy0un9/hyprplay.git
cd hyprplay
qmake6 hyprplay.pro
make -j$(nproc)
sudo make install
```

Then launch **Hyprplay** from your app menu, or run `hyprplay`.

Config lives in `~/.config/hyprplay/`. Library paths default to `~/Music`.

## Shortcuts

| Key | Action |
| --- | --- |
| `/` | Search library |
| `Space` | Play / pause |
| `↑` `↓` `←` `→` | Browse (→ plays the highlighted track) |
| `J` / `K` | Next / previous track |
| `?` | Full shortcut list |
| `Ctrl+,` | Settings |

## License

[MIT](LICENSE)
