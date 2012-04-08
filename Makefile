
APP = ipfwtabled
SRC = ipfwtabled.c ipfw.c

OBJS = ${SRC:.c=.o}

all: ${APP}

${APP}: ${OBJS}
	cc -o ipfwtabled ${OBJS}

clean:
	rm -fv *.d *.o ${APP}

.SUFFIXES: .c

.c.o:
	cc -c -MD -Wall $<

