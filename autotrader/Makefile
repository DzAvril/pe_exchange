CC=gcc
CFLAGS=-Wall -g

all: pe_exchange pe_trader

pe_exchange: pe_exchange.o 
	$(CC) $(CFLAGS) -o pe_exchange pe_exchange.o 

pe_exchange.o: pe_exchange.c pe_exchange.h pe_common.h
	$(CC) $(CFLAGS) -c pe_exchange.c

pe_trader: pe_trader.o 
	$(CC) $(CFLAGS) -o pe_trader pe_trader.o 

pe_trader.o: pe_trader.c pe_trader.h pe_common.h
	$(CC) $(CFLAGS) -c pe_trader.c

clean:
	rm -f *.o pe_exchange pe_trader
