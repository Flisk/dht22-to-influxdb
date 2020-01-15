CFLAGS := $(CFLAGS) -O2 -lwiringPi -lcurl

dht22: dht22.c

install: dht22
	install dht22 /usr/local/sbin

uninstall:
	-rm /usr/local/sbin/dht22

.PHONY: install uninstall
