#include "pe_trader.h"

#include <stdio.h>
#include <stdlib.h>

#include "pe_common.h"

// global variables
int trader_id;
int exchange_fd;
int trader_fd;
int order_id;
char exchange_fifo[MAX_FIFO_NAME_LENGTH];
char trader_fifo[MAX_FIFO_NAME_LENGTH];

void sig_handler(int signum) {
  if (signum == SIGUSR1) {
    char buf[1024];
    size_t len = read(exchange_fd, buf, sizeof(buf));
    printf("Received message: %.*s\n", (int)len, buf);
  }
}

void teardown() {
  printf("[debug] %s-%d Trader %d tear down resource.\n", __FILE__, __LINE__, trader_id);
  close(exchange_fd);
  close(trader_fd);
  unlink(exchange_fifo);
  unlink(trader_fifo);
}

int main(int argc, char** argv) {
  printf("Hello World from pe trader!\n");
  order_id = 0;
  if (argc > 1) {
    // get trader id from argv
    trader_id = atoi(argv[1]);
    printf("[debug] %s-%d trader_id : %d.\n", __FILE__, __LINE__, trader_id);
  }
  // set signal handler
  signal(SIGUSR1, sig_handler);

  // connect to exchange fifo
  char exchange_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(exchange_fifo, FIFO_PATH_EXCHANGE_PREFIX, trader_id);
  exchange_fd = open(exchange_fifo, O_RDONLY);

  // connect to trader fifo
  char trader_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(trader_fifo, FIFO_PATH_TRADER_PREFIX, trader_id);
  trader_fd = open(trader_fifo, O_WRONLY);
  sleep(5);
  teardown();
  // pause();
  return 0;
}