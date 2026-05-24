CFLAGS = -Wall -Wextra -O2
LDLIBS = -lm $(shell pkg-config --cflags --libs ncursesw)

all: maze3d

engine.o: engine.c engine.h gentex.h skull.h
	$(CC) $(CFLAGS) -c engine.c $(LDLIBS)

gentex.o: gentex.c gentex.h
	$(CC) $(CFLAGS) -c gentex.c

enemies.o: enemies.c enemies.h engine.h
	$(CC) $(CFLAGS) -c enemies.c

enemy_sprite_data.o: enemy_sprite_data.c enemy_sprite_data.h
	$(CC) $(CFLAGS) -c enemy_sprite_data.c

gameplay.o: gameplay.c gameplay.h engine.h enemies.h
	$(CC) $(CFLAGS) -c gameplay.c

maze3d-text.o: maze3d-text.c gameplay.h engine.h enemies.h
	$(CC) $(CFLAGS) -c maze3d-text.c $(LDLIBS)

maze3d: maze3d.c maze3d-text.o engine.o gentex.o enemies.o gameplay.o enemy_sprite_data.o
	$(CC) $(CFLAGS) -o $@ maze3d.c maze3d-text.o engine.o gentex.o enemies.o gameplay.o enemy_sprite_data.o $(LDLIBS)

clean:
	rm -f maze3d maze3d.exe -- *.o
	$(MAKE) -C $(PDCURSES_DIR)/wincon clean 2>/dev/null || true

superclean: clean
	rm -rf *.deb *.rpm

install: all
	install -d $(DESTDIR)/usr/bin
	install -m 755 maze3d $(DESTDIR)/usr/bin/maze3d

uninstall:
	rm -f $(DESTDIR)/usr/bin/maze3d

# ── Windows cross-compile (mingw-w64) ────────────────────────────
WINCC    ?= x86_64-w64-mingw32-gcc
WINAR    ?= x86_64-w64-mingw32-ar
WINCFLAGS ?= -Wall -Wextra -O2 -DPDC_WIDE -DPDC_FORCE_UTF8
PDCURSES_DIR = lib/pdcursesmod
PDCURSES_LIB = $(PDCURSES_DIR)/wincon/pdcurses.a

$(PDCURSES_LIB):
	$(MAKE) -C $(PDCURSES_DIR)/wincon _w64=1 WIDE=Y UTF8=Y CC=$(WINCC) AR=$(WINAR)

win: $(PDCURSES_LIB)
	$(WINCC) $(WINCFLAGS) -I$(PDCURSES_DIR) -I$(PDCURSES_DIR)/wincon \
		-o maze3d.exe maze3d.c maze3d-text.c engine.c gentex.c \
		enemies.c gameplay.c enemy_sprite_data.c \
		$(PDCURSES_LIB) -lm -lwinmm

# ── Distribution packages ──────────────────────────────────────────
VERSION := $(shell git describe --tags --dirty 2>/dev/null | sed 's/^v//; s/-/./g' || echo "0.1")
ARCH   := $(shell dpkg --print-architecture 2>/dev/null || echo "amd64")

deb: maze3d
	rm -rf /tmp/maze3d-deb
	mkdir -p /tmp/maze3d-deb/DEBIAN /tmp/maze3d-deb/usr/bin
	install -m 755 maze3d /tmp/maze3d-deb/usr/bin/maze3d
	printf 'Package: maze3d\n' > /tmp/maze3d-deb/DEBIAN/control
	printf 'Version: $(VERSION)\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Section: games\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Priority: optional\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Architecture: $(ARCH)\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Maintainer: oli4vr\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Depends: libncursesw6\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf 'Description: 2.5D terminal raycaster\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf ' A procedurally generated maze explored via DDA raycasting\n' >> /tmp/maze3d-deb/DEBIAN/control
	printf ' with Unicode block characters and xterm-256color.\n' >> /tmp/maze3d-deb/DEBIAN/control
	fakeroot dpkg-deb --build /tmp/maze3d-deb maze3d_$(VERSION)_$(ARCH).deb
	rm -rf /tmp/maze3d-deb

rpm: maze3d
	rm -rf /tmp/maze3d-rpm
	mkdir -p /tmp/maze3d-rpm/BUILD /tmp/maze3d-rpm/RPMS/x86_64 /tmp/maze3d-rpm/SPECS /tmp/maze3d-rpm/SOURCES
	cp maze3d /tmp/maze3d-rpm/SOURCES/
	printf 'Summary: 2.5D terminal raycaster\n' > /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'Name: maze3d\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'Version: $(VERSION)\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'Release: 1\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'License: MIT\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'Group: Amusements/Games\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'BuildArch: x86_64\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'Requires: ncurses\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'AutoReqProv: no\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%description\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'A procedurally generated maze explored via DDA raycasting\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'with Unicode block characters and xterm-256color.\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%prep\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%setup -c -T\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%install\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'mkdir -p %%{buildroot}/usr/bin\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf 'install -m 755 %%{_sourcedir}/maze3d %%{buildroot}/usr/bin/maze3d\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%clean\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '%%files\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	printf '/usr/bin/maze3d\n' >> /tmp/maze3d-rpm/SPECS/maze3d.spec
	rpmbuild -bb --define "_topdir /tmp/maze3d-rpm" \
		--define "_sourcedir /tmp/maze3d-rpm/SOURCES" \
		--define "_specdir /tmp/maze3d-rpm/SPECS" \
		--define "_builddir /tmp/maze3d-rpm/BUILD" \
		--define "_rpmdir /tmp/maze3d-rpm/RPMS" \
		/tmp/maze3d-rpm/SPECS/maze3d.spec
	mv /tmp/maze3d-rpm/RPMS/x86_64/maze3d-$(VERSION)-1.x86_64.rpm .
	rm -rf /tmp/maze3d-rpm

.PHONY: all clean install uninstall win deb rpm
