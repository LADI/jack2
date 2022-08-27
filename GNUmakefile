TOP:=$(shell pwd)

.PHONY: jack2dbus
jack2dbus:
	rm -rf jack2
	git clone --recurse-submodules --shallow-submodules ../jack2 jack2
	cd jack2 && python3 ./waf configure --prefix=$(TOP)/destdir/usr
	cd jack2 && python3 ./waf
	cd jack2 && python3 ./waf install
	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:$(TOP)/destdir/usr/lib/pkgconfig python3 ./waf configure --prefix=$(TOP)/destdir/usr
	python3 ./waf
	python3 ./waf install
