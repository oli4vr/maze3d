CFLAGS = -Wall -Wextra -O2
LDLIBS = -lm $(shell pkg-config --cflags --libs ncursesw)

all: maze3d

engine.o: engine.c engine.h gentex.h skull.h
	$(CC) $(CFLAGS) -c engine.c $(LDLIBS)

gentex.o: gentex.c gentex.h
	$(CC) $(CFLAGS) -c gentex.c

enemies.o: enemies.c enemies.h engine.h
	$(CC) $(CFLAGS) -c enemies.c

gameplay.o: gameplay.c gameplay.h engine.h enemies.h
	$(CC) $(CFLAGS) -c gameplay.c

maze3d-text.o: maze3d-text.c gameplay.h engine.h enemies.h
	$(CC) $(CFLAGS) -c maze3d-text.c $(LDLIBS)

maze3d: maze3d.c maze3d-text.o engine.o gentex.o enemies.o gameplay.o
	$(CC) $(CFLAGS) -o $@ maze3d.c maze3d-text.o engine.o gentex.o enemies.o gameplay.o $(LDLIBS)

clean:
	rm -f maze3d *.o

install: all
	install -d $(DESTDIR)/usr/bin
	install -m 755 maze3d $(DESTDIR)/usr/bin/maze3d

uninstall:
	rm -f $(DESTDIR)/usr/bin/maze3d

.PHONY: all clean install uninstall
