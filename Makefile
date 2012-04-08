
APP = ipfwtabled
SRC = ipfwtabled.c ipfw.c

OBJS = ${SRC:.c=.o}

all: ${OBJS}
	cc -o ipfwtabled ${OBJS}

clean:
	rm -fv *.d *.o

.SUFFIXES: .c

.c.o:
	cc -c -MD -Wall $<

