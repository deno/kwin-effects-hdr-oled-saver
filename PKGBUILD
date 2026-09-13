# Maintainer: deno <deno@users.noreply.github.com>

pkgname=kwin-effects-hdr-oled-saver
pkgver=0.9.0
pkgrel=1
pkgdesc="Dims the Plasma panel brightness when idle to prevent OLED burn-in"
arch=('x86_64' 'aarch64')
url="https://github.com/deno/kwin-effects-hdr-oled-saver"
license=('GPL-2.0-or-later')
depends=('kwin' 'qt6-base' 'qt6-declarative' 'libepoxy' 'libdrm')
makedepends=('cmake' 'extra-cmake-modules' 'ninja' 'vulkan-headers' 'kcmutils' 'kconfig' 'kcoreaddons' 'ki18n' 'kwindowsystem' 'wayland')
source=("${pkgname}-${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -B build -S "${pkgname}-${pkgver}/kwin-effect" -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DKDE_INSTALL_PLUGINDIR=/usr/lib/qt6/plugins \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build
}

package() {
    DESTDIR="${pkgdir}" cmake --install build
}
