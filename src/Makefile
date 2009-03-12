CC	= gcc
CFLAGS	= -D_GNU_SOURCE -O2 -pipe -Wall
#CFLAGS = -D_GNU_SOURCE -O -g -DDEBUG -Wall
LDFLAGS	= -Wl,-warn-common

all:	su-wrapper

su-wrapper:	su-wrapper.o
	$(CC) $(LDFLAGS) su-wrapper.o -o su-wrapper

install:	su-wrapper	
	install -s -o root -g root -m 4711 su-wrapper /usr/sbin/

clean:
	rm -f su-wrapper su-wrapper.o

uninstall:
	rm -f /usr/sbin/su-wrapper
	mv /etc/su-wrapper.conf /etc/su-wrapper.conf.---
