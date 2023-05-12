CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak
LDFLAGS=-lm
BINARIES=pe_trader

all: ${BINARIES}

.PHONE: clean
clean:
	rm -rf ${BINARIES}