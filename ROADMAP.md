# Qt Music — Roadmap

Prioritized backlog from a full app review (2026-09-07). The player is already a capable local FLAC/Opus app with libmpv, MPRIS2, Omarchy theming, Discogs/beets/lyrics tooling, and a recent UI polish pass — but playlists are half-wired, a few desktop contracts are incomplete, and several config/API stubs are orphaned.

Items are grouped by urgency:

| Priority | Meaning |
| --- | --- |
| **Need** | Broken, misleading, or blocking everyday use |
| **Should** | Important for a solid, trustworthy player |
| **Nice** | Delight, stretch goals, long-term architecture |

---

## Need

Things that should be fixed before treating the app as “done” for daily use.

### Product holes

1. ~~**Wire “Add to playlist” in the UI**~~ **Done**
   Browse track/album context menus call `App.addTrackToPlaylist` / `App.addAlbumToPlaylist` → `PlaylistService::addTracksToPlaylist`. Empty-playlist hint updated.

2. ~~**Clarify or fix dual install paths**~~ **Done**
   Primary `PREFIX` is `/usr/local` (qmake default, README, AGENTS). Optional
   `PREFIX=$HOME/.local`. Launcher warns if both binaries exist.

3. ~~**Add a LICENSE file**~~ **Done**
   MIT `LICENSE` matches AppStream `project_license`.

4. ~~**Finish or discard the Album panel header tweak**~~ **Done**
   Kept: removed redundant centered `"Album"` label above artwork.

### Correctness / reliability

5. ~~**Fix MPRIS `HasTrackList`**~~ **Done**
   Returns `false` — no TrackList adaptor is exported.

6. ~~**Stop blocking the UI on Discogs / metadata search**~~ **Done**
   Both services use main-thread `QNetworkAccessManager` + `finished` callbacks (no
   nested `QEventLoop`). Album Discogs dialog opens immediately and fills when search
   completes.

---

## Should

Work that makes qt-music feel complete and dependable as a daily driver.

### Playback & library

1. ~~**Play queue UI**~~ **Won’t do**
   Queue stays internal: users play albums/playlists; no “up next” panel. Shuffle/repeat
   stay on the now-playing bar.

2. ~~**Proper shuffle**~~ **Done**
   Fisher–Yates bag over the current album/playlist queue; current track pinned first when
   enabling shuffle; reshuffle (avoid immediate repeat) when the bag empties under queue repeat.

3. ~~**Library change detection**~~ **Done**
   Filesystem watcher on library dirs with debounced incremental rescan (`modified_time`).
   Settings → **Watch library for changes** (`library_watch`, default on).

4. ~~**Import review step**~~ **Done**
   Per-album checkboxes (skip/confirm), Opus destination preview, Import selected only.

### Desktop integration

5. ~~**Complete MPRIS Player properties**~~ **Done**
   `LoopStatus` / `Shuffle` (read+write + PropertiesChanged); `Seek` / `SetPosition` / `mpris:length` in microseconds; root `OpenUri` (`file://` → play + Raise).

6. ~~**Accessibility pass**~~ **Done**
   `Accessible.name` / roles on nav tabs, list delegates, now-playing (seek/volume), empty states, context menus, import rows, and major dialogs.

7. ~~**Secrets hardening**~~ **Done**
   Shared `SecretsStore`: prefer FreeDesktop Secret Service (libsecret) for Discogs/Genius tokens; fall back to `secrets.toml` with mode `0600`. Existing plaintext tokens migrate into the keyring on load.

### Code hygiene

8. ~~**Remove or implement orphans**~~ **Done**
   Removed unused `EqualizerConfig` and leftover sidebar layout keys (`sidebar_width` / `sidebar_collapsed`). README layout example updated for top-bar nav.

9. ~~**Minimal automated tests**~~ **Done**
   `tests/` Qt Test suite: FuzzyMatch, LRC parser, M3U round-trip, lyrics provider parsers.
   GitHub Actions CI runs unit tests + offscreen QML smoke (`QT_QPA_PLATFORM=offscreen timeout 5 ./qt-music-bin`).

10. ~~**Playlist UX beyond “add one track”**~~ **Done**
    Multi-select in Browse (Ctrl/Shift/Ctrl+A) → add selection to playlist; album context menu add; drag track-number handle to reorder playlist tracks.

11. ~~**README / AppStream screenshot**~~ **Done**
    Real library + now-playing shot at `docs/assets/library-browse.png`.

---

## Nice

Stretch features and polish once Need/Should are in good shape.

### Formats & library model

1. ~~Broader format support (MP3/AAC/M4A)~~ **Done in 0.1** — or document FLAC/Opus-only as a permanent product choice
2. Richer DB schema: genre, year, album-artist, and browse/filter by them
3. Cover-art fetch that does not require a full Discogs token flow (embedded MP3/M4A art extraction)
4. Drag-and-drop files/folders into the library or import inbox

### Desktop & playback extras

5. Optional desktop notifications on track change (`org.freedesktop.Notifications`)
6. System tray / mini-player window
7. Sleep timer
8. ReplayGain modes and crossfade / gapless controls in Settings (today hardcoded / disabled in mpv options)
9. ~~File associations / `MimeType` in the `.desktop` entry~~ **Done in 0.1**
10. Scrobbling (ListenBrainz or Last.fm)

### Lyrics & metadata

11. In-app lyrics editor and clearer offset UX (beyond `lyrics_offset_ms` in config)
12. Smart / dynamic playlists (e.g. recently added, by genre)
13. Playback history / recently played

### Architecture & packaging

14. Split `AppController` (~1.1k LOC god façade) into focused controllers (library, playback, dialogs/settings)
15. Replace hand-rolled TOML regex parsing with a real parser
16. Albums as a proper list model (not `QStringList`) for consistency with artists/tracks
17. CMake migration and/or CI (build + `git diff --check` + offscreen smoke)
18. Manual color themes for non-Omarchy desktops
19. Public AUR package + GitHub release process (PKGBUILD landed; publish after tag)

---

## Current strengths (keep)

Do not lose these while chasing the backlog:

- Clear service boundaries under `src/core/` with QML-facing APIs
- libmpv playback, gapless-friendly seeking, optional DAC passthrough
- Fuzzy library search, keyboard-first Browse navigation, shortcut help (`?`)
- Waveform seek bar, karaoke LRC lyrics with multi-provider fetch
- MPRIS2 Raise / Quit / transport (single-instance via Raise)
- Omarchy theme live-reload and icon tinting
- Tag editing (TagLib, atomic writes), Discogs artist/album tooling, beets-aware import
- Desktop packaging: launcher, `.desktop`, icons, AppStream metainfo

---

## Suggested order of attack

1. Wire **Add to playlist** (Need #1) — unlocks a core workflow with little new backend.
2. Fix **MPRIS HasTrackList** and **async Discogs** (Need #5–6) — correctness and freeze risk.
3. **LICENSE** + install-path docs + AlbumPanel commit (Need #2–4) — cheap hygiene.
4. **Proper shuffle** (Should) — everyday playback quality. Queue UI skipped (internal only).
5. README screenshot (Should #11); playlist UX polish done.
6. Pick Nice items by personal taste (tray, notifications, formats, scrobbling).

---

## Key code map

| Path | Role |
| --- | --- |
| `src/core/AppController.*` | QML façade (`App`) |
| `src/core/PlaybackService.*` | mpv queue, repeat, shuffle, volume |
| `src/core/LibraryService.*` | SQLite index (`.flac` / `.opus` only) |
| `src/core/PlaylistService.*` | M3U playlists; includes unwired `addTrackToPlaylist` |
| `src/core/DiscogsService.*` | Blocking search loops to async-ify |
| `src/mpris/MprisPlayer.*` | Desktop media control |
| `src/ui/qml/Main.qml` | Shell, nav, shortcuts |
| `src/ui/qml/views/*.qml` | Browse, Playlists, Import, Settings |
| `AGENTS.md` | Build/install rules for agents and humans |

When an item ships, move it out of this file (or tick it) and mention it in commit messages / a future `CHANGELOG.md`.
