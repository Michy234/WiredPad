.PHONY: all check install clean package
BUILD ?= build
PREFIX ?= /usr/local
all:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	cmake --build $(BUILD) -j2
check: all
	ctest --test-dir $(BUILD) --output-on-failure
install: all
	DESTDIR=$(DESTDIR) cmake --install $(BUILD)
package:
	cd packaging/arch && makepkg --force
clean:
	cmake -E remove_directory $(BUILD)
