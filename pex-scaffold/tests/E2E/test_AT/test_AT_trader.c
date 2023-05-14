#define _POSIX_C_SOURCE 199309L
#include "../../../pe_trader.h"

// global variables
extern int trader_id;
extern int exchange_fd;
extern int trader_fd;
extern int order_id;
extern char exchange_fifo[MAX_FIFO_NAME_LENGTH];
extern char trader_fifo[MAX_FIFO_NAME_LENGTH];

void sig_handler(int signum) {
  if (signum == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    memset(buf, '\0', sizeof(buf));
    read(exchange_fd, buf, sizeof(buf));
    printf("%s Received message from exchange: %s.\n", LOG_EXCHANGE_PREFIX, buf);
    if ((strncmp(buf, RESPONSE_PREFIX, strlen(RESPONSE_PREFIX)) == 0) &&
        (strncmp(buf, MESSAGE_MARKET_OPEN, strlen(MESSAGE_MARKET_OPEN)) != 0)) {
      Response* response = (Response*)malloc(sizeof(Response));
      deserialize_response(buf, response);
      handle_exchange_reponse(response);
      free(response);
      teardown_trader();
      exit(0);
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Not enough arguments\n");
    return 1;
  }
  if (argc > 1) {
    trader_id = atoi(argv[1]);
  }

  // register signal handler
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sig_handler;
  sigaction(SIGUSR1, &sa, NULL);

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
}
