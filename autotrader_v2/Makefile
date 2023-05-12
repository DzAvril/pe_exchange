CC=gcc
CFLAGS=-Wall -g

all: pe_trader

pe_trader: pe_trader.o 
	$(CC) $(CFLAGS) -o pe_trader pe_trader.o 

pe_trader.o: pe_trader.c pe_trader.h pe_common.h
	$(CC) $(CFLAGS) -c pe_trader.c

clean:
	rm -f *.o pe_trader
