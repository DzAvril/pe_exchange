# build and run auto-trader
``` bash
cd autotrader
make
./pe_exchange ./products.txt ./trader_1 ./trader_2 ./trader_3
```

# Auto-trader process

- Read products from products.txt
- Exchange try to connect to fifo pipes of traders
- Exchange sen "MARKET OPEN" to all traders if all traders are connected
- Exchange send random message such as "MARKET SELL switch 776 87;" to all traders
- Traders read message "MARKET SELL switch 776 87;" from fifo pipes
- Traders place a BUY order "BUY 0 switch 776 87;" to exchange
