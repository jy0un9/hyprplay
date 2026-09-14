# Maintainer: jy0un9 <https://github.com/jy0un9>
pkgname=qt-music
pkgver=0.1.0
pkgrel=1
pkgdesc="Native Qt 6 music player for local FLAC/Opus/MP3/M4A libraries (Omarchy/Hyprland)"
arch=('x86_64')
url="https://github.com/jy0un9/qt-music"
license=('MIT')
depends=(
  'qt6-base'
  'qt6-declarative'
  'mpv'
  'taglib'
  'libsecret'
  'ffmpeg'
)
makedepends=('qt6-tools')
optdepends=(
  'beets: optional autotag during import'
  'opus-tools: FLAC→Opus convert import mode (opusenc)'
)
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cd "$pkgname-$pkgver"
  qmake6 PREFIX=/usr qt-music.pro
  make
}

package() {
  cd "$pkgname-$pkgver"
  make INSTALL_ROOT="$pkgdir" install
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
