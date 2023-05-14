CC = gcc
CFLAGS = -Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak
LDFLAGS = -lm

# Define variables for common compilation and linking flags
COMPILE = $(CC) $(CFLAGS) -c $< -o $@
LINK = $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: pe_exchange pe_trader

pe_exchange: pe_exchange.o pe_exchange_function.o
	$(LINK)

pe_exchange.o: pe_exchange.c pe_exchange.h pe_exchange_function.h
	$(COMPILE)

pe_exchange_function.o: pe_exchange_function.c pe_exchange_function.h pe_common.h
	$(COMPILE)

pe_trader: pe_trader.o
	$(LINK)

pe_trader.o: pe_trader.c pe_trader.h pe_common.h
	$(COMPILE)

clean:
	rm -f *.o pe_exchange pe_trader
