#define _POSIX_C_SOURCE 199309L
#include "pe_trader.h"

// global variables
int trader_id;
int exchange_fd;
int trader_fd;
int order_id = 0;
char exchange_fifo[MAX_FIFO_NAME_LENGTH];
char trader_fifo[MAX_FIFO_NAME_LENGTH];

void sig_handler(int signum) {
  if (signum == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    memset(buf, '\0', sizeof(buf));
    read(exchange_fd, buf, sizeof(buf));
    // printf("Received message: %.*s\n", (int)len, buf);
    // fflush(stdout);
    if ((strncmp(buf, RESPONSE_PREFIX, strlen(RESPONSE_PREFIX)) == 0) &&
        (strncmp(buf, MESSAGE_MARKET_OPEN, strlen(MESSAGE_MARKET_OPEN)) != 0)) {
      Response* response = (Response*)malloc(sizeof(Response));
      deserialize_response(buf, response);
      handle_exchange_reponse(response);
      free(response);
    }
  }
}

int main(int argc, char** argv) {
  // printf(" %s:%d hello world from trader\n", __FILE__, __LINE__);
  if (argc < 2) {
    printf("Not enough arguments\n");
    return 1;
  }
  if (argc > 1) {
    trader_id = atoi(argv[1]);
  }

  // register signal handler
  // signal(SIGUSR1, sig_handler);
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sig_handler;
  sigaction(SIGUSR1, &sa, NULL);
  // connect to named pipes

  // connect to exchange fifo
  char exchange_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(exchange_fifo, FIFO_EXCHANGE, trader_id);
  exchange_fd = open(exchange_fifo, O_RDONLY);

  // connect to trader fifo
  char trader_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(trader_fifo, FIFO_TRADER, trader_id);
  trader_fd = open(trader_fifo, O_WRONLY);
  // event loop:
  while (1) {
    pause();
  }
  return 0;
  // wait for exchange update (MARKET message)
  // send order
  // wait for exchange confirmation (ACCEPTED message)
}
