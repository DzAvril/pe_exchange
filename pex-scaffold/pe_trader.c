#include "pe_trader.h"

// global variables
int trader_id;
int exchange_fd;
int trader_fd;
int order_id = 0;
char exchange_fifo[MAX_FIFO_NAME_LENGTH];
char trader_fifo[MAX_FIFO_NAME_LENGTH];
int timestamp = 0;

void teardown() {
  close(exchange_fd);
  close(trader_fd);
  unlink(exchange_fifo);
  unlink(trader_fifo);
  timestamp++;
  printf("[%s %d] [t=%d] Event: DISCONNECT\n", LOG_TRADER_PREFIX, trader_id, timestamp);
}

void deserialize_response(char* buf, Response* response) {
  const char token[2] = " ";

  char* pos = strchr(buf, ';');
  if (pos != NULL) {
    memset(pos, '\0', strlen(pos));
  }

  char* ptr[RESPONSE_LETTERS_NUM];
  ptr[0] = strtok(buf, token);
  int i = 0;
  while (i < RESPONSE_LETTERS_NUM && ptr[i] != NULL) {
    i++;
    ptr[i] = strtok(NULL, token);
  }
  if (strncmp(ptr[1], ORDER_TYPE_BUY, strlen(ORDER_TYPE_BUY)) == 0) {
    response->type = BUY;
  } else if (strncmp(ptr[1], ORDER_TYPE_SELL, strlen(ORDER_TYPE_SELL)) == 0) {
    response->type = SELL;
  } else if (strncmp(ptr[1], ORDER_TYPE_AMEND, strlen(ORDER_TYPE_AMEND)) == 0) {
    response->type = AMEND;
  } else if (strncmp(ptr[1], ORDER_TYPE_CANCEL, strlen(ORDER_TYPE_CANCEL)) == 0) {
    response->type = CANCEL;
  } else {
    printf("[debug] %s-%d Unknown order type : %s.\n", __FILE__, __LINE__, ptr[1]);
    exit(0);
  }
  strcpy(response->product.name, ptr[2]);
  response->quantity = atoi(ptr[3]);
  response->price = atoi(ptr[4]);
}

void serialize_order(Order* order, char* buf) {
  char type_string[MAX_TYPE_STRING_LENGTH];
  switch (order->type) {
    case BUY:
      strncpy(type_string, ORDER_TYPE_BUY, MAX_TYPE_STRING_LENGTH);
      break;
    case SELL:
      strncpy(type_string, ORDER_TYPE_SELL, MAX_TYPE_STRING_LENGTH);
      break;
    case AMEND:
      strncpy(type_string, ORDER_TYPE_AMEND, MAX_TYPE_STRING_LENGTH);
      break;
    case CANCEL:
      strncpy(type_string, ORDER_TYPE_CANCEL, MAX_TYPE_STRING_LENGTH);
      break;
    default:
      break;
  }
  sprintf(buf, "%s %d %s %d %d;", type_string, order->order_id, order->product.name,
          order->quantity, order->price);
}

void send_message_to_exchange(char* buf) {
  printf("[PEX-Milestone] Trader -> Exchange: %s\n", buf);
  size_t message_len = strlen(buf);
  if (write(trader_fd, buf, message_len) != message_len) {
    perror("Error writing message to trader");
    exit(EXIT_FAILURE);
  }
  // send signal to trader
  if (kill(getppid(), SIGUSR1) == -1) {
    perror("Error sending signal to trader");
    exit(EXIT_FAILURE);
  }
}

void handle_exchange_reponse(Response* response) {
  Order* order = (Order*)malloc(sizeof(Order));
  order->order_id = order_id++;
  order->type = BUY;
  strcpy(order->product.name, response->product.name);
  order->quantity = response->quantity;
  order->price = response->price;
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  serialize_order(order, buf);
  send_message_to_exchange(buf);
  free(order);
  teardown();
}

void sig_handler(int signum) {
  if (signum == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    memset(buf, '\0', sizeof(buf));
    size_t len = read(exchange_fd, buf, sizeof(buf));
    printf("[%s %d] [t=%d]Received from PEX: %.*s\n", LOG_TRADER_PREFIX, trader_id, timestamp,
           (int)len, buf);
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
  // event loop:
  // while (1) {
  //   pause();
  // }
  sleep(2);
  teardown();
  return 0;
}
