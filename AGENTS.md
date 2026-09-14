# hyprplay

Qt6/QML player. Target `hyprplay-bin`, launcher `hyprplay`.

## Build / install

Installed copy is under `/usr/local` — a repo build alone will not update the app you launch.

**After every change, the agent must build and install** (not just compile in-tree):

```bash
cd /home/jy0un9/Documents/qt-music
qmake6 hyprplay.pro
make -j$(nproc)
pkexec make -C /home/jy0un9/Documents/qt-music install
```

(or `sudo make install` from the repo root)

If `pkexec`/`sudo` cannot authenticate in-session, ask the user to approve the prompt / run install, then continue.

Optional: `PREFIX=$HOME/.local`. Prefer one prefix only.

QML is in `resources/hyprplay.qrc` — rebuild after QML edits.
Do not set properties the QML base type lacks (e.g. `AbstractButton` has no `flat`).

## Check

```bash
git diff --check
cd tests && qmake6 tests.pro && make -j$(nproc) && ./hyprplay-tests && cd ..
QT_QPA_PLATFORM=offscreen timeout 5 ./hyprplay-bin
```
