# hyprplay

Qt6/QML music player (qmake build, target `hyprplay-bin`, launcher script `hyprplay`).

## Build & install (do this after ALL changes)

The globally launched app is the installed copy, not the repo binary. A repo
build alone will make it look like "nothing changed".

**Primary install prefix is `/usr/local`.**

```bash
cd /home/jy0un9/Documents/qt-music
qmake6 hyprplay.pro
make -j$(nproc)
sudo make install   # requires fingerprint/password in an interactive terminal
```

Then restart the app (`/usr/local/bin/hyprplay` or app launcher).

Notes:
- `sudo` cannot authenticate inside the agent session; the user runs
  `sudo make install` themselves.
- Optional no-sudo install: `qmake6 hyprplay.pro PREFIX=$HOME/.local && make -j$(nproc) && make install`.
  Prefer only one install. If both exist, the launcher warns; remove the user
  copy with `qmake6 hyprplay.pro PREFIX=$HOME/.local && make uninstall`.
- Library DB, config, playlists, and cover art live under your home/music dirs —
  reinstall never deletes them.
- QML is compiled into the binary via `resources/hyprplay.qrc`; always rebuild
  after editing QML.
- QML components in `src/ui/qml/components/` must not use properties their base
  type lacks (e.g. `AbstractButton` has no `flat`) or the whole app fails to
  load with "QQmlApplicationEngine failed to load component".

## Version sync (releases)

Keep these equal when cutting a release:
- `kAppVersion` in `src/main.cpp`
- `desktop/org.jy0un9.hyprplay.metainfo.xml` `<release>`
- `PKGBUILD` `pkgver`
- `CHANGELOG.md` heading

## Verification

```bash
git diff --check
cd tests && qmake6 tests.pro && make -j$(nproc) && ./hyprplay-tests && cd ..
QT_QPA_PLATFORM=offscreen timeout 5 ./hyprplay-bin   # check for QML load errors
desktop-file-validate desktop/hyprplay.desktop
appstreamcli validate desktop/org.jy0un9.hyprplay.metainfo.xml
```
