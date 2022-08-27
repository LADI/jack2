TOP:=$(shell pwd)

.PHONY: jack2dbus
jack2dbus:
        cd jack2 && python3 ./waf configure --prefix=/usr
        cd jack2 && python3 ./waf
        cd jack2 && python3 ./waf install --destdir=$(TOP)/destdir
        python3 ./waf configure --prefix=/usr
        python3 ./waf
        python3 ./waf install --destdir=$(TOP)/destdir
