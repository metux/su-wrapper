
all:	su-wrapper

su-wrapper:	su-wrapper.o
	$(CC) su-wrapper.o -o su-wrapper $(LD_FLAGS)

install:	su-wrapper	
	cp su-wrapper /usr/sbin
	chown root.root /usr/sbin/su-wrapper
	chmod +s /usr/sbin/su-wrapper

clean:
	rm -f su-wrapper su-wrapper.o

uninstall:
	rm -f /usr/sbin/su-wrapper
	mv /etc/su-wrapper.conf /etc/su-wrapper.conf.---
