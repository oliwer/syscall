PROG     := syscall
VER      := 1.1
CFLAGS   ?= -std=c99 -Os -Wall -Wextra -pedantic
CPPFLAGS += -DVERSION=\"$(VER)\"
PREFIX   ?= /usr/local
MANDIR   ?= $(PREFIX)/share/man

all: $(PROG) $(PROG).1 README.pod

$(PROG): systab.h
	$(CC) $(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -o $@ $@.c

systab.h:
	sh lssyscalls | awk '{printf "{\"%s\", %d},\n", $$1, $$2}' > $@

$(PROG).1: $(PROG).pod
	pod2man -c "" -n $(PROG) -r $(VER) -s 1 $< $@

README.pod: $(PROG).pod
	cp -f $< $@

check: $(PROG)
	@./tests.sh

install: all
	install -D -m 755 $(PROG) $(DESTDIR)$(PREFIX)/bin
	install -D -m 644 $(PROG).1 $(DESTDIR)$(MANDIR)/man1

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(PROG) \
	      $(DESTDIR)$(MANDIR)/man1/$(PROG).1
clean:
	$(RM) $(PROG) systab.h

tarball: clean
	cd .. && cp -rf $(PROG) $(PROG)-$(VER) && \
	tar czf $(PROG)-$(VER).tar.gz $(PROG)-$(VER) && \
	rm -rf $(PROG)-$(VER)
