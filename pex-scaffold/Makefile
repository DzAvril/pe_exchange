CC = gcc
CFLAGS = -Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak
LDFLAGS = -lm

# Define variables for common compilation and linking flags
COMPILE = $(CC) $(CFLAGS) -c $< -o $@
LINK = $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

all: pe_exchange pe_trader
test: pe_exchange pe_trader unit_test \
./tests/E2E/test_AT/test_AT_trader \
./tests/E2E/test_AT/test_AT_exchange \
./tests/E2E/test_buy_sell/test_buy_sell_exchang \
./tests/E2E/test_buy_sell/test_buy_sell_trader

pe_exchange: pe_exchange.o pe_exchange_function.o
	$(LINK)

pe_exchange.o: pe_exchange.c pe_exchange.h pe_exchange_function.h
	$(COMPILE)

pe_exchange_function.o: pe_exchange_function.c pe_exchange_function.h pe_common.h
	$(COMPILE)

pe_trader: pe_trader.o pe_trader_function.o
	$(LINK)

pe_trader.o: pe_trader.c pe_trader.h pe_trader_function.h
	$(COMPILE)

pe_trader_function.o: pe_trader_function.c pe_trader_function.h pe_common.h
	$(COMPILE)

unit_test: unit_test.o pe_exchange_function.o pe_trader_function.o
	$(LINK) -lcmocka

unit_test.o: ./tests/unit-tests.c pe_exchange_function.h pe_trader_function.h ./tests/cmocka.h
	$(COMPILE)

./tests/E2E/test_AT/test_AT_exchange: ./tests/E2E/test_AT/test_AT_exchange.o pe_exchange_function.o
	$(LINK)

./tests/E2E/test_AT/test_AT_exchange.o: ./tests/E2E/test_AT/test_AT_exchange.c pe_exchange.h pe_exchange_function.c pe_exchange_function.h
	$(COMPILE)

./tests/E2E/test_AT/test_AT_trader: ./tests/E2E/test_AT/test_AT_trader.o pe_trader_function.o
	$(LINK)

./tests/E2E/test_AT/test_AT_trader.o: ./tests/E2E/test_AT/test_AT_trader.c pe_trader.h pe_trader_function.c pe_trader_function.h
	$(COMPILE)

./tests/E2E/test_buy_sell/test_buy_sell_exchang: ./tests/E2E/test_buy_sell/test_buy_sell_exchang.o pe_exchange_function.o
	$(LINK)

./tests/E2E/test_buy_sell/test_buy_sell_exchang.o: ./tests/E2E/test_buy_sell/test_buy_sell_exchange.c pe_exchange.h pe_exchange_function.c pe_exchange_function.h
	$(COMPILE)

./tests/E2E/test_buy_sell/test_buy_sell_trader: ./tests/E2E/test_buy_sell/test_buy_sell_trader.o pe_trader_function.o
	$(LINK)

./tests/E2E/test_buy_sell/test_buy_sell_trader.o: ./tests/E2E/test_buy_sell/test_buy_sell_trader.c pe_trader.h pe_trader_function.c pe_trader_function.h
	$(COMPILE)

PHONY: run_test
run_test:
	./unit_test

clean:
	rm -f *.o unit_test pe_exchange pe_trader ./tests/E2E/test_AT/*.o ./tests/E2E/test_AT/test_AT_exchange ./tests/E2E/test_AT/test_AT_trader ./tests/E2E/test_buy_sell/*.o ./tests/E2E/test_buy_sell/test_buy_sell_exchang ./tests/E2E/test_buy_sell/test_buy_sell_trader
