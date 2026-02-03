CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
LDFLAGS ?=
LDLIBS ?= -lncurses

TARGET = tasci
SRC = TASCI.c colours_fix.c
OBJ = $(SRC:.c=.o)

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
FONTDIR ?= $(DATADIR)/fonts/TTF
FONTFILE ?= fonts/Hack-Regular.ttf

.PHONY: all clean install install-pacman install-debian test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	@echo "No tests defined."

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	@if [ -f "$(FONTFILE)" ]; then \
		install -d $(DESTDIR)$(FONTDIR); \
		install -m 644 $(FONTFILE) $(DESTDIR)$(FONTDIR)/; \
	fi

# Pacman-specific install
install-pacman: install
	fc-cache -f

# Debian-specific install
install-debian: install
	fc-cache -f
