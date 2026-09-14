# Test suite

Run from the repo:

```bash
cd tests
qmake6 tests.pro
make -j$(nproc)
HYPRPLAY_TEST_AO=null QT_QPA_PLATFORM=offscreen ./hyprplay-tests
```

## Coverage map (areas 1–11)

| Area | Test file(s) |
| --- | --- |
| 1 Library | `tst_library.cpp`, `tst_audio_formats.cpp`, `tst_fuzzymatch.cpp` |
| 2 Playback | `tst_playback.cpp` |
| 3 Now playing / media | `tst_track_media.cpp`, `tst_lrcparser.cpp` |
| 4 Playlists | `tst_playlists.cpp`, `tst_playlist_m3u.cpp` |
| 5 Import | `tst_import.cpp` |
| 6 Tags | `tst_tags.cpp` |
| 7 Lyrics | `tst_lyrics_service.cpp`, `tst_lyrics_parsers.cpp` |
| 8 Discogs / enrichment helpers | `tst_discogs_rank.cpp`, `tst_metadata_live.cpp` |
| 9 Settings / config / secrets | `tst_desktop_cli_theme.cpp` |
| 10 Desktop / CLI / MPRIS | `tst_desktop_cli_theme.cpp` |
| 11 Theme | `tst_desktop_cli_theme.cpp` |
| Format matrix E2E | `tst_format_matrix.cpp` |

## Live network tests

Offline CI skips live provider calls. To exercise MusicBrainz, Discogs, LRCLIB, NetEase, and lyrics.ovh:

```bash
export HYPRPLAY_LIVE_NETWORK=1
export DISCOGS_TOKEN=your_token   # required for Discogs search
HYPRPLAY_TEST_AO=null QT_QPA_PLATFORM=offscreen ./hyprplay-tests
```
