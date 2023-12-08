# Maintainer: Devin J. Pohly <djpohly+arch@gmail.com>
pkgname=dwl
pkgver=0.5
pkgrel=0
pkgdesc="Simple, hackable dynamic tiling Wayland compositor (dwm for Wayland)"
arch=('x86_64')
url="https://github.com/djpohly/dwl"
license=('GPL')
depends=('wlroots>=0.17')
makedepends=('wayland-protocols')
optdepends=('xorg-xwayland: for XWayland support')

prepare() {
	cd ../
	# Use a custom config.h if the file is not empty
	if [ -s "$srcdir/config.h" ]; then
		cp -f "$srcdir/config.h" config.h
	fi
	# Uncomment to compile with XWayland support
	#sed -i -e '/-DXWAYLAND\|xcb/s/^#//' config.mk
}

build() {
	cd ../
	make
}

package() {
	cd ../
	make PREFIX="$pkgdir/usr" install
	# Delete src folder created by makepkg if empty and unneeded
	[ -z "$(ls -A ./src)" ] && rmdir ./src
}

