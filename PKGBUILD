# Maintainer: Labfox <labfoxdev@gmail.com>
# libfprint fork with an added driver for the Goodix 27c6:55a4 sensor
# (goodix5xx): TLS-PSK transport + SIFT-based (SIGFM/OpenCV) matching.

pkgname=libfprint-goodix5xx-git
pkgver=1.94.5
pkgrel=1
pkgdesc='libfprint with an added driver for the Goodix 27c6:55a4 fingerprint sensor (SIGFM matching)'
arch=('x86_64')
url='https://github.com/Labfox/libfprint'
license=('LGPL-2.1-or-later')
depends=('glib2' 'libgusb' 'libgudev' 'nss' 'pixman' 'openssl' 'opencv')
makedepends=('meson' 'git' 'gobject-introspection' 'gtk3')
provides=("libfprint=$pkgver" 'libfprint-2.so')
conflicts=('libfprint' 'libfprint-goodix5xx')
replaces=('libfprint-goodix5xx')
options=('!lto')
_branch='goodix5xx-55a4-support'
source=("libfprint::git+https://github.com/Labfox/libfprint.git#branch=${_branch}")
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
