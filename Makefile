CFLAGS += -std=gnu99 -Wall -Wwrite-strings -pthread
PREFIX ?= /usr/local

XMMS_CFLAGS := `pkg-config xmms2-client --cflags`
XMMS_LDFLAGS := `pkg-config xmms2-client --libs`
CURL_CFLAGS := `pkg-config libcurl --cflags`
CURL_LDFLAGS := `pkg-config libcurl --libs`

ifndef VERBOSE
	QUIET_CC = @echo '    ' CC $@;
	QUIET_LINK = @echo '    ' LINK $@;
endif

OBJECTS := src/xmms2-scrobbler.o src/queue.o src/strbuf.o src/md5.o \
           src/submission.o

all: src/xmms2-scrobbler

install: src/xmms2-scrobbler
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 src/xmms2-scrobbler $(DESTDIR)$(PREFIX)/bin

src/xmms2-scrobbler: $(OBJECTS)
	$(QUIET_LINK)$(CC) $(XMMS_LDFLAGS) $(CURL_LDFLAGS) $(OBJECTS) -o $@

src/%.o : src/%.c
	$(QUIET_CC)$(CC) $(CFLAGS) $(XMMS_CFLAGS) $(CURL_CFLAGS) -o $@ -c $<

ChangeLog:
	git log > ChangeLog

clean:
	rm -f $(OBJECTS) src/xmms2-scrobbler
