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

.PHONY: all clean install uninstall win
