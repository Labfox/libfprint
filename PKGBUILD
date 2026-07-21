# Maintainer: Labfox <labfoxdev@gmail.com>
# libfprint fork with an added driver for the Goodix 27c6:55a4 sensor
# (goodix5xx), ported from goodix-fp-dump.

pkgname=libfprint-goodix5xx
pkgver=1.94.5
pkgrel=1
pkgdesc='Library for fingerprint readers (with Goodix 27c6:55a4 goodix5xx driver)'
arch=('x86_64')
url='https://gitlab.freedesktop.org/libfprint/libfprint'
license=('LGPL-2.1-or-later')
depends=('glib2' 'libgusb' 'libgudev' 'nss' 'pixman' 'openssl')
makedepends=('meson' 'git' 'gobject-introspection' 'gtk3')
provides=("libfprint=$pkgver" 'libfprint-2.so')
conflicts=('libfprint')
# Build from the local checkout / branch that carries the driver.
_repo='/home/labfox/Git/libfprint'
_branch='goodix5xx-55a4-support'
source=("libfprint::git+file://${_repo}#branch=${_branch}")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/libfprint"
  printf '1.94.5.r%s.g%s' \
    "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  local srcdir_repo="$srcdir/libfprint"

  meson setup "$srcdir_repo" build \
    --prefix=/usr \
    --buildtype=plain \
    --wrap-mode=nodownload \
    -Ddrivers=default \
    -Dintrospection=true \
    -Ddoc=false \
    -Dgtk-examples=false \
    -Dudev_hwdb=enabled \
    -Dudev_rules=enabled
  meson compile -C build
}

package() {
  meson install -C build --destdir "$pkgdir"
}
