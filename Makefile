CFLAGS := $(CFLAGS) -lwiringPi -lcurl -lfl

SOURCES := dht22.c dht22.h \
	   config.tab.c lex.yy.c

BINDIR ?= $(DESTDIR)/bin
ETCDIR ?= $(DESTDIR)/etc

dht22: $(SOURCES)

config.tab.c config.tab.h: config.y
	bison --defines $<

lex.yy.c: config.lex config.tab.h
	lex $<

.PHONY: install uninstall destdir

install: dht22 destdir
	install -D dht22 $(BINDIR)/dht22-to-influxdb
	install -D --mode=600 dht22-to-influxdb.conf $(ETCDIR)

uninstall: destdir
	-rm $(BINDIR)/dht22-to-influxdb
	-rm $(ETCDIR)/dht22-to-influxdb.conf

destdir:
	@test -n "$(DESTDIR)" \
		|| (>&2 echo "error: DESTDIR is not defined" && exit 1)

