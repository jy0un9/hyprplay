# Hyprplay

![Platform](https://img.shields.io/badge/platform-Linux-blue)
![Qt](https://img.shields.io/badge/Qt-6-41cd52)
![License](https://img.shields.io/badge/license-MIT-blue)

A local music player for Hyprland and Omarchy. Point it at your music folder and
it browses by artist, album, and track, plays through mpv, shows synced lyrics,
and picks up the colors of your current Omarchy theme.

![Library browser](docs/assets/browse.png)

## Features

- Artist → album → track browsing with instant search (`/`)
- Plays FLAC, Opus, Ogg Vorbis, MP3, M4A, and AAC
- Waveform seek bar and karaoke-style synced lyrics, fetched automatically
- Playlists, tag editing, and cover art straight from your files
- Bit-perfect output for USB DACs, plus media keys and controls via MPRIS
- Colors follow your Omarchy theme, no restart needed

## Install

On Arch or Omarchy, install from the AUR:

```bash
yay -S hyprplay
```

Then launch **Hyprplay** from your app menu, or run `hyprplay`.

## First run

Open **Settings** (`Ctrl+,`) and set **Music folders** to wherever your music
lives — `~/Music` by default. Hit **Rescan** and the library fills in.

![Settings](docs/assets/settings.png)

Everything saves the moment you change it. If you want new files picked up
automatically, turn on **Watch for changes**.

## Shortcuts

| Key | Action |
| --- | --- |
| `/` | Search library |
| `Space` | Play / pause |
| `↑` `↓` `←` `→` | Browse (→ plays the highlighted track) |
| `J` / `K` | Next / previous track |
| `Ctrl+N` | New playlist |
| `Ctrl+,` | Settings |
| `?` | Full shortcut list |

## Extras

Under **Settings → Tools**:

- **Import** — copy loose downloads from an inbox folder into your library
- **Convert** — turn FLAC albums into Opus to save space
- **Enrich library** — fill in missing tags and cover art from Discogs

## License

[MIT](LICENSE)
