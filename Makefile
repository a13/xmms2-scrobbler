CFLAGS += -std=gnu99 -Wall -Wwrite-strings -pthread
ENDIAN_CFLAGS=
PREFIX ?= /usr/local

VERSION := 0.3.9
TARBALL := xmms2-scrobbler-$(VERSION).tar.gz

XMMS_CFLAGS := `pkg-config xmms2-client --cflags`
XMMS_LDFLAGS := `pkg-config xmms2-client --libs`
CURL_CFLAGS := `pkg-config libcurl --cflags`
CURL_LDFLAGS := `pkg-config libcurl --libs`

ifndef VERBOSE
	QUIET_CC = @echo '    ' CC $@;
	QUIET_LINK = @echo '    ' LINK $@;
	QUIET_MKDIR = @echo '    ' MKDIR $@;
endif

ifdef BIG_ENDIAN
	ENDIAN_CFLAGS=-DWORDS_BIGENDIAN
endif

BINARY := bin/xmms2-scrobbler
OBJECTS := src/xmms2-scrobbler.o \
           src/list.o \
           src/queue.o \
           src/strbuf.o \
           src/md5.o \
           src/submission.o

all: $(BINARY)

install: $(BINARY)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BINARY) $(DESTDIR)$(PREFIX)/bin

$(BINARY): $(OBJECTS) bin
	$(QUIET_LINK)$(CC) $(LDFLAGS) $(XMMS_LDFLAGS) $(CURL_LDFLAGS) $(OBJECTS) -o $@

src/%.o : src/%.c
	$(QUIET_CC)$(CC) $(CFLAGS) $(XMMS_CFLAGS) $(CURL_CFLAGS) $(ENDIAN_CFLAGS) -o $@ -c $<

bin:
	$(QUIET_MKDIR)mkdir bin

dist:
	rm -rf $(TARBALL) xmms2-scrobbler-$(VERSION)
	git archive --format=tar --prefix=xmms2-scrobbler-$(VERSION)/ HEAD | tar -x
	git log > xmms2-scrobbler-$(VERSION)/ChangeLog
	tar czvf $(TARBALL) xmms2-scrobbler-$(VERSION)
	rm -rf xmms2-scrobbler-$(VERSION)

clean:
	rm -rf $(OBJECTS) bin
