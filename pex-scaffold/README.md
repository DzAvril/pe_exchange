1. Describe how your exchange works.
- At the beginning of the program, parse_args function is used to read parameters from the command line. The first parameter is the product file, followed by the trader program.
- Use sigaction to set the signal handling function sig_handler, which is used to process the SIGUSR1 signal sent by traders.
- Use fork_child_process function to fork the corresponding number of child processes for the trader programs.
- At the same time, create the corresponding number of pipes for communication between traders and exchanges, and connect them to the pipes.
- The pipe /tmp/exchange_${trader_id} is used by the exchange to send messages to the trader with the ID of `trader_id`. The pipe /tmp/trader_${trader_id} is used by the exchange to receive messages from the trader with the ID of `trader_id`.
- On the contrary, the trader with the ID of `trader_id` sends messages to the exchange through the pipe /tmp/trader_${trader_id}, and receives messages from the exchange through the pipe /tmp/exchange_${trader_id}.
- After initialization is complete, send the message "MARKET OPEN" to all traders.
- Use epoll to listen to events of pipe closure on traders, and if any trader closes the pipe, output "Trader ${trader_id} disconnected" and set the disconnected flag of that trader to 1.
- When a trader sends a SIGUSR1 signal to the exchange, the signal handling function processes it and parses the command using the parsing_command function whether it is a BUY/SELL command or not.
- For BUY/SELL commands, validate_order is used to check whether the order is valid, such as whether the quantity is 0, and so on.
- If the order is valid, match_orders is used to match the order. If a match is found, the FILL message is sent to the trader, and the orderbook and positions information are updated.
- After each valid order is processed, print_orderbook and print_position functions are used to print the current orderbook and positions information.
- When all pipes are detected to be closed and all child processes exit, clean up all pipes, print the broker fee received for this transaction, and exit the program.
- To compile the program, run the make command in the root directory, and then run the program with the command ./pe_exchange ./products.txt ./trader_1 ./trader_2 ./trader_3


2. Describe your design decisions for the trader and how it's fault-tolerant.
- The trader program is a child process of the exchange. The exchange sends the "MARKET OPEN" message to the trader after the trader is connected. The trader receives the message and starts to send the BUY/SELL command to the exchange.
- The trader program uses the sigaction function to set the signal handling function sig_handler, which is used to process the SIGUSR1 signal sent by the exchange.
- For the implementation of auto trader function, when the signal handling function receives the "MARKET SELL" event, it automatically sends the corresponding number of BUY orders to the exchange through the handle_exchange_reponse function.
- The trader program is relatively simple and does not require too much fault tolerance mechanism. I check each time the pipe is opened in the program. If the pipe is opened unsuccessfully, the error message is output and the program exits.

3. Describe your tests and how to run them.
- Testing is divided into unit testing and E2E (End-to-End) testing.
- Specifically, the following functions are tested in unit testing: deserialize_response, serialize_order, load_products, parse_args, deserialize_order, get_trader_by_id, get_trader_by_fd, get_trader_by_pid, fork_child_process.
- To compile the unit tests, use the `make test` command. To run the unit tests, use the `make run_test` command. The output of the test results is shown below:
``` bash
make test
make run_test
[==========] Running 7 test(s).
[ RUN      ] test_deserialize_response
[       OK ] test_deserialize_response
[ RUN      ] test_serialize_order
[       OK ] test_serialize_order
[ RUN      ] test_load_products
[PEX] Trading 4 products: GPU CPU SWITCH TV
[       OK ] test_load_products
[ RUN      ] test_parse_args
[PEX] Trading 3 products: Basil Coriander Chives
[       OK ] test_parse_args
[ RUN      ] test_deserialize_order
[       OK ] test_deserialize_order
[ RUN      ] test_trader_operator
[       OK ] test_trader_operator
[ RUN      ] test_general
[       OK ] test_general
[==========] 7 test(s) run.
[  PASSED  ] 7 test(s).
```
- The E2E testing includes the testing of the normal sending of BUY/SELL orders between the auto trader and exchange with traders. The directory where the auto trader is tested is located at `tests/E2E/test_AT,` and the directory where the exchange and trader are tested is located at `tests/E2E/test_buy_sell`.
- To compile the E2E tests, use the make test command. The command to execute each test is as follows:
- For the auto trader test:
``` bash
cd tests/E2E/test_AT
./test_AT_exchange ./products.txt ./test_AT_trader
```
The output is:
``` bash
[PEX] Received message from exchange: MARKET OPEN;.
[PEX] Received message from exchange: MARKET SELL GPU 356 318;.
[PEX] Starting
[PEX] Trading 4 products: GPU CPU Memory switch
[PEX] Created FIFO /tmp/pe_exchange_0
[PEX] Created FIFO /tmp/pe_trader_0
[PEX] Connected to /tmp/pe_exchange_0
[PEX] Connected to /tmp/pe_trader_0
[PEX] Received message from trader: BUY 0 GPU 356 318;.
[PEX] Trader 0 disconnected
[PEX] Trading completed
[PEX] Exchange fees collected: $0
```
- Trading test between exchange and trader
``` bash
cd tests/E2E/test_buy_sell
./test_buy_sell_exchang ./products.txt ./test_buy_sell_trader  ./test_buy_sell_trader
```
- In the 'tests/E2E/test_buy_sell' directory, trader_0.txt and trader_1.txt are the input data for trader_0 and trader_1, respectively, and the output is as follows:
``` bash
[PEX] Starting
[PEX] Trading 2 products: GPU Router
[PEX] Created FIFO /tmp/pe_exchange_0
[PEX] Created FIFO /tmp/pe_trader_0
[PEX] Starting trader 0 (./test_buy_sell_trader)
[PEX] Connected to /tmp/pe_exchange_0
[PEX] Connected to /tmp/pe_trader_0
[PEX] [T0] Parsing command: <BUY 0 GPU 30 500>
[Trader 0] [t=0]Received from PEX: ACCEPTED 0;
[PEX]   --ORDERBOOK--
[PEX]   Product: GPU; Buy levels: 1; Sell levels: 0
[PEX]           BUY 30 @ $500 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 0 ($0), Router 0 ($0)
[PEX]   Trader 1: GPU 0 ($0), Router 0 ($0)
[PEX] [T0] Parsing command: <BUY 1 GPU 30 501>
[PEX]   --ORDERBOOK--
[Trader 0] [t=0]Received from PEX: ACCEPTED 1;
[PEX]   Product: GPU; Buy levels: 2; Sell levels: 0
[PEX]           BUY 30 @ $501 (1 order)
[PEX]           BUY 30 @ $500 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 0 ($0), Router 0 ($0)
[PEX]   Trader 1: GPU 0 ($0), Router 0 ($0)
[PEX] [T0] Parsing command: <BUY 2 GPU 30 501>
[PEX]   --ORDERBOOK--
[Trader 0] [t=0]Received from PEX: ACCEPTED 2;
[PEX]   Product: GPU; Buy levels: 2; Sell levels: 0
[PEX]           BUY 60 @ $501 (2 orders)
[PEX]           BUY 30 @ $500 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 0 ($0), Router 0 ($0)
[PEX]   Trader 1: GPU 0 ($0), Router 0 ($0)
[PEX] [T0] Parsing command: <BUY 3 GPU 30 502>
[PEX]   --ORDERBOOK--
[Trader 0] [t=0]Received from PEX: ACCEPTED 3;
[PEX]   Product: GPU; Buy levels: 3; Sell levels: 0
[PEX]           BUY 30 @ $502 (1 order)
[PEX]           BUY 60 @ $501 (2 orders)
[PEX]           BUY 30 @ $500 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 0 ($0), Router 0 ($0)
[PEX]   Trader 1: GPU 0 ($0), Router 0 ($0)
[PEX] Created FIFO /tmp/pe_exchange_1
[PEX] Created FIFO /tmp/pe_trader_1
[PEX] Starting trader 1 (./test_buy_sell_trader)
[PEX] Connected to /tmp/pe_exchange_1
[PEX] Connected to /tmp/pe_trader_1
[PEX] [T1] Parsing command: <SELL 0 GPU 99 501>
[Trader 0] [t=0]Received from PEX: MARKET OPEN;
[Trader 0] [t=0]Received from PEX: MARKET SELL GPU 99 501;
[Trader 1] [t=0]Received from PEX: ACCEPTED 0;
[Trader 0] [t=0]Received from PEX: FILL 3 30;
[PEX] Match: Order 3 [T0], New Order 0 [T1], value: $15060, fee: $151.
[Trader 1] [t=0]Received from PEX: FILL 0 30;
[Trader 0] [t=0]Received from PEX: FILL 1 30;
[PEX] Match: Order 1 [T0], New Order 0 [T1], value: $15030, fee: $150.
[Trader 1] [t=0]Received from PEX: FILL 0 30;
[Trader 0] [t=0]Received from PEX: FILL 2 30;
[PEX] Match: Order 2 [T0], New Order 0 [T1], value: $15030, fee: $150.
[Trader 1] [t=0]Received from PEX: FILL 0 30;
[PEX]   --ORDERBOOK--
[PEX]   Product: GPU; Buy levels: 1; Sell levels: 1
[PEX]           SELL 9 @ $501 (1 order)
[PEX]           BUY 30 @ $500 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 90 ($-45120), Router 0 ($0)
[PEX]   Trader 1: GPU -90 ($44669), Router 0 ($0)
[PEX] [T1] Parsing command: <SELL 1 GPU 99 402>
[Trader 0] [t=0]Received from PEX: MARKET SELL GPU 99 402;FILL 0 30;
[Trader 1] [t=0]Received from PEX: ACCEPTED 1;
[PEX] Match: Order 0 [T0], New Order 1 [T1], value: $15000, fee: $150.
[PEX]   --ORDERBOOK--
[Trader 1] [t=0]Received from PEX: FILL 1 30;
[PEX]   Product: GPU; Buy levels: 0; Sell levels: 2
[PEX]           SELL 9 @ $501 (1 order)
[PEX]           SELL 69 @ $402 (1 order)
[PEX]   Product: Router; Buy levels: 0; Sell levels: 0
[PEX]   --POSITIONS--
[PEX]   Trader 0: GPU 120 ($-60120), Router 0 ($0)
[PEX]   Trader 1: GPU -120 ($59519), Router 0 ($0)
```