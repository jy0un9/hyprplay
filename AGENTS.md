# qt-music

Qt6/QML music player (qmake build, target `qt-music-bin`, launcher script `qt-music`).

## Build & install (do this after ALL changes)

The globally launched app is the installed copy, not the repo binary. A repo
build alone will make it look like "nothing changed".

**Primary install prefix is `/usr/local`.**

```bash
cd /home/jy0un9/Documents/qt-music
qmake6 qt-music.pro
make -j$(nproc)
sudo make install   # requires fingerprint/password in an interactive terminal
```

Then restart the app (`/usr/local/bin/qt-music` or app launcher).

Notes:
- `sudo` cannot authenticate inside the agent session; the user runs
  `sudo make install` themselves.
- Optional no-sudo install: `qmake6 qt-music.pro PREFIX=$HOME/.local && make -j$(nproc) && make install`.
  Prefer only one install. If both exist, the launcher warns; remove the user
  copy with `qmake6 qt-music.pro PREFIX=$HOME/.local && make uninstall`.
- Library DB, config, playlists, and cover art live under your home/music dirs —
  reinstall never deletes them.
- QML is compiled into the binary via `resources/qt-music.qrc`; always rebuild
  after editing QML.
- QML components in `src/ui/qml/components/` must not use properties their base
  type lacks (e.g. `AbstractButton` has no `flat`) or the whole app fails to
  load with "QQmlApplicationEngine failed to load component".

## Version sync (releases)

Keep these equal when cutting a release:
- `kAppVersion` in `src/main.cpp`
- `desktop/org.jy0un9.qt-music.metainfo.xml` `<release>`
- `PKGBUILD` `pkgver`
- `CHANGELOG.md` heading

## Verification

```bash
git diff --check
cd tests && qmake6 tests.pro && make -j$(nproc) && ./qt-music-tests && cd ..
QT_QPA_PLATFORM=offscreen timeout 5 ./qt-music-bin   # check for QML load errors
desktop-file-validate desktop/qt-music.desktop
appstreamcli validate desktop/org.jy0un9.qt-music.metainfo.xml
```
