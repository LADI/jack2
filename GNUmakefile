TOP:=$(shell pwd)

.PHONY: jack2dbus
jack2dbus:
	rm -rf jack2
	git clone -b $(shell git symbolic-ref --short HEAD) --recurse-submodules --shallow-submodules https://github.com/LADI/jack2
	cd jack2 && python3 ./waf configure --prefix=$(TOP)/destdir/usr
	cd jack2 && python3 ./waf
	cd jack2 && python3 ./waf install
	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:$(TOP)/destdir/usr/lib/pkgconfig python3 ./waf configure --prefix=$(TOP)/destdir/usr
	python3 ./waf
	python3 ./waf install

README.html: README.adoc GNUmakefile
	asciidoc -b html5 -a data-uri -a icons --theme ladi -o README.html README.adoc

.PHONY: AUTHORS.regenerate
AUTHORS.regenerate:
	git shortlog -sn -- wscript ./dbus/* linux/* man/* ./doc/* | sed -E 's@^\s+\S+\s+(.+)@\1@' > AUTHORS

.PHONY: doc/jackdbus.html
doc/jackdbus.html:
	asciidoc -b html5 -a icons -a data-uri --theme ladi -o doc/jackdbus.html README.adoc
