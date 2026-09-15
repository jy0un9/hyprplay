# Changelog

## 0.1.3

- Trim the Audio device picker to real outputs (PipeWire/Pulse sinks, or ALSA
  cards in DAC mode), hide unplugged HDMI, and show short nicknames
- Bit-perfect mode uses exclusive ALSA `hw:` (USB DAC preferred), verifies
  source→DAC sample rate, and no longer falls back to PipeWire
- Bit-perfect borrows the DAC via PipeWire profile off and restores it when
  the mode is turned off or the app exits (so it reappears in OS settings)
- Toggling bit-perfect resumes the current track at the same position (both on and off)
- Now-playing bar: output picker next to volume (devices + bit-perfect toggle)
- Now-playing controls grouped as track / transport / sound, with a distinct device icon
- Transport uses a foreground play disc with repeat/shuffle as side modifiers
- Light Omarchy themes: chrome bars and unfilled icons use contrast-safe colors
  (play disc already contrasted; skip/repeat/shuffle/volume/device now match)

## 0.1.2

- Fix lyrics UI not reloading after a successful fetch (false “already present”)
- Artists column header shows the selected artist name
- Remove Genius lyrics provider; expand test suite
- Remove beets integration
- Split Import (copy into library) from Convert (FLAC→Opus) with separate settings
- Redesigned Settings: flat grouped rows matching the rest of the UI, Preferences/Tools
  navigation, live pipeline state for Enrich, and settings that save as you change them
  instead of behind a Save button
- Simpler README for install and everyday use

## 0.1.1

- Public GitHub repo
- Open audio files from the CLI / file manager (`Exec %U`, single-instance OpenUri handoff)
- Ogg Vorbis (`.ogg` / `.oga`) library support; embedded art for MP3 / M4A / AAC / Ogg
- Genius lyrics provider removed (never provided synced lyrics)
- Library enrichment marked optional/advanced (Discogs token required)
- Product copy lists supported formats (CLI, desktop, AppStream)
- Library column header polish

## 0.1.0

- Renamed to Hyprplay
- FLAC / Opus / MP3 / M4A / AAC
- Import: copy by default, optional Opus convert
- Library enrichment (Discogs, title fix, lyrics)
