CC	= gcc
#CFLAGS	= -D_GNU_SOURCE -O2 -pipe -Wall
CFLAGS = -D_GNU_SOURCE -g
LDFLAGS	= -Wl,-warn-common

all:	su-wrapper

su-wrapper:	su-wrapper.o
	$(CC) $(LDFLAGS) su-wrapper.o -o su-wrapper

install:	su-wrapper	
	cp su-wrapper /usr/sbin
	chown root.root /usr/sbin/su-wrapper
	chmod +s /usr/sbin/su-wrapper

clean:
	rm -f su-wrapper su-wrapper.o

uninstall:
	rm -f /usr/sbin/su-wrapper
	mv /etc/su-wrapper.conf /etc/su-wrapper.conf.---
