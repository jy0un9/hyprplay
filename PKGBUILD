# Maintainer: jy0un9 <https://github.com/jy0un9>
pkgname=hyprplay
pkgver=0.1.1
pkgrel=1
pkgdesc="Local music player for Hyprland/Omarchy"
arch=('x86_64')
url="https://github.com/jy0un9/hyprplay"
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
sha256sums=('02196ee11c281226788bec1ef852a3fc7198a8745f28fbdc344a46eed4fc2de7')

build() {
  cd "$pkgname-$pkgver"
  qmake6 PREFIX=/usr hyprplay.pro
  make
}

package() {
  cd "$pkgname-$pkgver"
  make INSTALL_ROOT="$pkgdir" install
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
