
APP = ipfwtabled
SRC = ipfwtabled.c ipfw.c

OBJS = ${SRC:.c=.o}

all: ${APP}

${APP}: ${OBJS}
	cc -o ipfwtabled ${OBJS}

install: all
	install -o root -g wheel -m 555 ipfwtabled /usr/local/sbin
	install -o root -g wheel -m 555 ipfwtabled.sh /usr/local/etc/rc.d/ipfwtabled

deinstall:
	rm /usr/local/sbin/ipfwtabled
	rm /usr/local/etc/rc.d/ipfwtabled

clean:
	rm -fv *.d *.o ${APP}

.SUFFIXES: .c

.c.o:
	cc -c -MD -Wall $<

