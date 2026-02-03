# Maintainer: Stefan Tasic <stefan@example.com>
pkgname=tasci
pkgver=0.1
pkgrel=1
pkgdesc="TASCI - A lightweight terminal-based text editor"
arch=('x86_64' 'i686')
url="https://github.com/tasic928/tasci"
license=('MIT')
depends=('ncurses')
makedepends=('gcc' 'make')
source=("$pkgname-$pkgver.tar.gz")
md5sums=('SKIP')

build() {
    cd "$pkgname-$pkgver"
    make
}

package() {
    cd "$pkgname-$pkgver"
    make install PREFIX=/usr
    install -Dm644 fonts/Hack-Regular.ttf -t "$pkgdir/usr/share/fonts/TTF/"
}
