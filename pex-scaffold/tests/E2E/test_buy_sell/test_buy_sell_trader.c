#include "../../../pe_trader.h"
#define MAX_LINES 1000
#define MAX_LINE_LEN 256
// global variables
extern int trader_id;
extern int exchange_fd;
extern int trader_fd;
extern int order_id;
extern char exchange_fifo[MAX_FIFO_NAME_LENGTH];
extern char trader_fifo[MAX_FIFO_NAME_LENGTH];
extern int timestamp;

void sig_handler(int signum) {
  if (signum == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    memset(buf, '\0', sizeof(buf));
    size_t len = read(exchange_fd, buf, sizeof(buf));
    printf("[%s %d] [t=%d]Received from PEX: %.*s\n", LOG_TRADER_PREFIX, trader_id, timestamp,
           (int)len, buf);
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
  char input_file[100];
  char lines[MAX_LINES][MAX_LINE_LEN];
  int line_num = 0;
  sprintf(input_file, "trader_%d.txt", trader_id);
  // check if trader.txt exists
  if (access(input_file, F_OK) != -1) {
    FILE* fp = fopen(input_file, "r");
    if (!fp) {
      perror("Failed to open file");
      exit(EXIT_FAILURE);
    }
    while (line_num < MAX_LINES && fgets(lines[line_num], MAX_LINE_LEN, fp)) {
      line_num++;
    }
  }

  // register signal handler
  // signal(SIGUSR1, sig_handler);
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sig_handler;
  sigaction(SIGUSR1, &sa, NULL);
  // sigaction(SIGINT, &sa, NULL);
  // connect to named pipes

  // connect to exchange fifo
  char exchange_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(exchange_fifo, FIFO_EXCHANGE, trader_id);
  exchange_fd = open(exchange_fifo, O_RDONLY);

  // connect to trader fifo
  char trader_fifo[MAX_FIFO_NAME_LENGTH];
  sprintf(trader_fifo, FIFO_TRADER, trader_id);
  trader_fd = open(trader_fifo, O_WRONLY);

  // send test data to exchange
  for (int i = 0; i < line_num; i++) {
    char buf[MAX_MESSAGE_LENGTH];
    memset(buf, '\0', sizeof(buf));
    strcpy(buf, lines[i]);
    send_message_to_exchange(buf);
    sleep(1);
  }
  // event loop:
  while (1) {
    pause();
  }
  return 0;
}
