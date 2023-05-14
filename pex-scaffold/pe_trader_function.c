#include "pe_trader_function.h"

// global variables
extern int trader_id;
extern int exchange_fd;
extern int trader_fd;
extern int order_id;
extern char exchange_fifo[MAX_FIFO_NAME_LENGTH];
extern char trader_fifo[MAX_FIFO_NAME_LENGTH];

void teardown() {
  close(exchange_fd);
  close(trader_fd);
  unlink(exchange_fifo);
  unlink(trader_fifo);
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
  // printf("[PEX-Milestone] Trader -> Exchange: %s\n", buf);
  //   fflush(stdout);
  size_t message_len = strlen(buf);
  if (write(trader_fd, buf, message_len) != message_len) {
    // perror("Error writing message to trader");
    exit(EXIT_FAILURE);
  }
  // send signal to trader
  if (kill(getppid(), SIGUSR1) == -1) {
    perror("Error sending signal to trader");
    exit(EXIT_FAILURE);
  }
}

void handle_exchange_reponse(Response* response) {
  if (response->type != SELL) {
    return;
  }
  Order* order = (Order*)malloc(sizeof(Order));
  order->order_id = order_id++;
  order->type = BUY;
  strcpy(order->product.name, response->product.name);
  order->quantity = response->quantity;
  if (order->quantity >= MAX_ORDER_QUANTITY) {
    free(order);
    exit(0);
  }
  order->price = response->price;
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  serialize_order(order, buf);
  send_message_to_exchange(buf);
  free(order);
  // teardown();
}