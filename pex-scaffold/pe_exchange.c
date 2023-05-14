#include "pe_exchange.h"

#include <math.h>
#include <sys/wait.h>
#include <time.h>
// Global variables
Product products[MAX_PRODUCTS];
int num_products = 0;
Trader traders[MAX_TRADERS];
int num_traders = MAX_TRADERS;
char* trader_path[MAX_TRADERS];
int exchange_fee_collected = 0;
Order orderbook[MAX_ORDERS];
int num_orders = 0;

void load_products(const char* filename) {
  FILE* file = fopen(filename, "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }
  fscanf(file, "%d", &num_products);
  for (int i = 0; i < num_products; i++) {
    fscanf(file, "%s", products[i].name);
  }
  fclose(file);
  char all_products[PRODUCT_NAME_LENGTH * MAX_PRODUCTS];
  memset(all_products, '\0', sizeof(all_products));
  for (int i = 0; i < num_products; i++) {
    strcat(all_products, " ");
    strcat(all_products, products[i].name);
  }
  printf("%s Trading %d products:%s\n", LOG_EXCHANGE_PREFIX, num_products, all_products);
}

void parse_args(int argc, char** argv) {
  const char* filename = argv[1];
  load_products(filename);
  num_traders = argc - 2;
  for (int i = 0; i < num_traders; i++) {
    trader_path[i] = argv[i + 2];
  }
}

void fork_child_process() {
  for (int i = 0; i < num_traders; i++) {
    sprintf(traders[i].exchange_fifo, FIFO_EXCHANGE, i);
    sprintf(traders[i].trader_fifo, FIFO_TRADER, i);
    // check if fifo already exists, remove it if so
    if (access(traders[i].exchange_fifo, F_OK) != -1) {
      if (remove(traders[i].exchange_fifo) != 0) {
        perror("Error removing FIFO");
        exit(EXIT_FAILURE);
      }
    }
    if (access(traders[i].trader_fifo, F_OK) != -1) {
      if (remove(traders[i].trader_fifo) != 0) {
        perror("Error removing FIFO");
        exit(EXIT_FAILURE);
      }
    }

    // create fifo
    if (mkfifo(traders[i].exchange_fifo, 0666) < 0) {
      perror("Error creating FIFO");
      exit(EXIT_FAILURE);
    }
    printf("%s Created FIFO %s\n", LOG_EXCHANGE_PREFIX, traders[i].exchange_fifo);

    if (mkfifo(traders[i].trader_fifo, 0666) < 0) {
      perror("Error creating FIFO");
      exit(EXIT_FAILURE);
    }
    printf("%s Created FIFO %s\n", LOG_EXCHANGE_PREFIX, traders[i].trader_fifo);

    // initialize trader positions
    for (int j = 0; j < num_products; j++) {
      traders[i].positions[j].product = products[j];
      traders[i].positions[j].quantity = 0;
      traders[i].positions[j].price = 0;
    }

    // fork a child process to run pe_trader
    pid_t pid = fork();
    if (pid == 0) {
      // child process
      char trader_id[20];
      sprintf(trader_id, "%d", i);
      printf("%s Starting trader %d (%s)\n", LOG_EXCHANGE_PREFIX, i, trader_path[i]);
      char* trader_argv[] = {trader_path[i], trader_id, NULL};
      execv(trader_argv[0], trader_argv);
    } else if (pid > 0) {
      // parent process
      traders[i].trader_id = i;
      traders[i].pid = pid;

      traders[i].exchange_fd = open(traders[i].exchange_fifo, O_WRONLY);
      if (traders[i].exchange_fd == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
      }
      printf("%s Connected to %s\n", LOG_EXCHANGE_PREFIX, traders[i].exchange_fifo);

      traders[i].trader_fd = open(traders[i].trader_fifo, O_RDONLY);
      if (traders[i].trader_fd == -1) {
        perror("Failed to open FIFO");
        exit(EXIT_FAILURE);
      }
      printf("%s Connected to %s\n", LOG_EXCHANGE_PREFIX, traders[i].trader_fifo);
    } else {
      perror("Error forking child process");
      exit(EXIT_FAILURE);
    }
  }
}

void teardown() {
  for (int i = 0; i < num_traders; i++) {
    close(traders[i].exchange_fd);
    close(traders[i].trader_fd);
    unlink(traders[i].exchange_fifo);
    unlink(traders[i].trader_fifo);
    remove(traders[i].trader_fifo);
    remove(traders[i].exchange_fifo);
  }
}

void send_message(int trader_id, const char* message) {
  size_t message_len = strlen(message);
  if (write(traders[trader_id].exchange_fd, message, message_len) != message_len) {
    perror("Error writing message to trader");
    exit(EXIT_FAILURE);
  }

  // send signal to trader
  if (kill(traders[trader_id].pid, SIGUSR1) == -1) {
    perror("Error sending signal to trader");
    exit(EXIT_FAILURE);
  }
}

void deserialize_order(Order* order, char* buf) {
  const char token[2] = " ";

  char* ptr[ORDER_LETTERS_NUM];
  ptr[0] = strtok(buf, token);
  int i = 0;
  while (i < ORDER_LETTERS_NUM && ptr[i] != NULL) {
    i++;
    ptr[i] = strtok(NULL, token);
  }
  if (strncmp(ptr[0], ORDER_TYPE_BUY, strlen(ORDER_TYPE_BUY)) == 0) {
    order->type = BUY;
  } else if (strncmp(ptr[0], ORDER_TYPE_SELL, strlen(ORDER_TYPE_SELL)) == 0) {
    order->type = SELL;
  } else if (strncmp(ptr[0], ORDER_TYPE_AMEND, strlen(ORDER_TYPE_AMEND)) == 0) {
    order->type = AMEND;
  } else if (strncmp(ptr[0], ORDER_TYPE_CANCEL, strlen(ORDER_TYPE_CANCEL)) == 0) {
    order->type = CANCEL;
  } else {
    printf("%s Unknown order type : %s\n", LOG_EXCHANGE_PREFIX, ptr[0]);
    exit(0);
  }
  order->order_id = atoi(ptr[1]);
  strcpy(order->product.name, ptr[2]);
  order->quantity = atoi(ptr[3]);
  order->price = atoi(ptr[4]);
}

int get_trader_by_id(int trader_id) {
  for (int i = 0; i < num_traders; i++) {
    if (traders[i].trader_id == trader_id) {
      return i;
    }
  }
  return -1;
}

int get_trader_by_pid(int pid) {
  for (int i = 0; i < num_traders; i++) {
    if (traders[i].pid == pid) {
      return i;
    }
  }
  return -1;
}

int get_trader_by_fd(int fd) {
  for (int i = 0; i < num_traders; i++) {
    if (traders[i].trader_fd == fd) {
      return i;
    }
  }
  return -1;
}

void print_report(Report* report) {
  printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", LOG_EXCHANGE_PREFIX,
         report->product.name, report->buy_level, report->sell_level);
  for (int i = 0; i < report->num_product; i++) {
    if (report->orderBrief[i].num_order > 1) {
      printf("%s\t\t%s %d @ $%d (%d orders)\n", LOG_EXCHANGE_PREFIX,
             report->orderBrief[i].type == BUY ? "BUY" : "SELL", report->orderBrief[i].quantity,
             report->orderBrief[i].price, report->orderBrief[i].num_order);
    } else {
      printf("%s\t\t%s %d @ $%d (%d order)\n", LOG_EXCHANGE_PREFIX,
             report->orderBrief[i].type == BUY ? "BUY" : "SELL", report->orderBrief[i].quantity,
             report->orderBrief[i].price, report->orderBrief[i].num_order);
    }
  }
  return;
}

void print_orderbook() {
  printf("%s\t--ORDERBOOK--\n", LOG_EXCHANGE_PREFIX);
  for (int i = 0; i < num_products; i++) {
    Report* report = (Report*)malloc(sizeof(Report));
    int current_buy_price = -1;
    int current_sell_price = -1;
    report->product = products[i];
    report->num_product = 0;
    report->buy_level = 0;
    report->sell_level = 0;
    // init report orderBrief num_order to 0
    for (int j = 0; j < MAX_ORDERS; j++) {
      report->orderBrief[j].num_order = 0;
    }
    OrderInfo orderInfo[MAX_ORDERS];
    int num_orderInfo = 0;
    for (int j = 0; j < num_orders; j++) {
      if (strcmp(orderbook[j].product.name, products[i].name) == 0) {
        orderInfo[num_orderInfo].index = j;
        orderInfo[num_orderInfo].quantity = orderbook[j].quantity;
        orderInfo[num_orderInfo].price = orderbook[j].price;
        orderInfo[num_orderInfo].type = orderbook[j].type;
        num_orderInfo++;
      }
    }
    // sout orderInfo by price desc, if price is the same, sout buy order type
    for (int i = 0; i < num_orderInfo - 1; i++) {
      for (int j = 0; j < num_orderInfo - i - 1; j++) {
        if (orderInfo[j].price < orderInfo[j + 1].price) {
          OrderInfo temp = orderInfo[j];
          orderInfo[j] = orderInfo[j + 1];
          orderInfo[j + 1] = temp;
        } else if (orderInfo[j].price == orderInfo[j + 1].price) {
          if (orderInfo[j].type == BUY && orderInfo[j + 1].type == SELL) {
            OrderInfo temp = orderInfo[j];
            orderInfo[j] = orderInfo[j + 1];
            orderInfo[j + 1] = temp;
          }
        }
      }
    }

    for (int j = 0; j < num_orderInfo; j++) {
      if (orderInfo[j].type == BUY) {
        if (current_buy_price != orderInfo[j].price) {
          report->buy_level++;
          report->orderBrief[report->num_product].type = BUY;
          report->orderBrief[report->num_product].price = orderInfo[j].price;
          report->orderBrief[report->num_product].quantity = orderInfo[j].quantity;
          report->orderBrief[report->num_product].num_order++;
          report->num_product++;
        } else {
          if (orderInfo[j].type == orderInfo[j - 1].type) {
            report->orderBrief[report->num_product - 1].quantity += orderInfo[j].quantity;
            report->orderBrief[report->num_product - 1].num_order++;
          }
        }
        current_buy_price = orderInfo[j].price;
      } else if (orderInfo[j].type == SELL) {
        if (current_sell_price != orderInfo[j].price) {
          report->sell_level++;
          report->orderBrief[report->num_product].type = SELL;
          report->orderBrief[report->num_product].price = orderInfo[j].price;
          report->orderBrief[report->num_product].quantity = orderInfo[j].quantity;
          report->orderBrief[report->num_product].num_order++;
          report->num_product++;
        } else {
          if (orderInfo[j].type == orderInfo[j - 1].type) {
            report->orderBrief[report->num_product - 1].quantity += orderInfo[j].quantity;
            report->orderBrief[report->num_product - 1].num_order++;
          }
        }
        current_sell_price = orderInfo[j].price;
      }
    }
    print_report(report);
    free(report);
  }
}

void print_position() {
  // print postions of each trader
  printf("%s\t--POSITIONS--\n", LOG_EXCHANGE_PREFIX);
  for (int i = 0; i < num_traders; i++) {
    char buf[MAX_LOG_LENGTH];
    memset(buf, '\0', sizeof(buf));
    sprintf(buf, "Trader %d: ", i);
    for (int j = 0; j < num_products; j++) {
      char temp[MAX_LOG_LENGTH];
      memset(temp, '\0', sizeof(temp));
      if (j != num_products - 1) {
        sprintf(temp, "%s %d ($%d), ", products[j].name, traders[i].positions[j].quantity,
                traders[i].positions[j].price);
      } else {
        sprintf(temp, "%s %d ($%d)", products[j].name, traders[i].positions[j].quantity,
                traders[i].positions[j].price);
      }
      strcat(buf, temp);
    }
    printf("%s\t%s\n", LOG_EXCHANGE_PREFIX, buf);
  }
}

void accpeted(int trader_id, int order_id) {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  sprintf(buf, "%s %d;", MESSAGE_ACCEPTED, order_id);
  send_message(trader_id, buf);
}

void serialize_response(Response* response, char* buf) {
  char type_string[MAX_TYPE_STRING_LENGTH];
  switch (response->type) {
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
  sprintf(buf, "%s %s %s %d %d;", RESPONSE_PREFIX, type_string, response->product.name,
          response->quantity, response->price);
}

void market_message(int trader_id, Order* order) {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  Response* res = (Response*)malloc(sizeof(Response));
  if (res == NULL) {
    perror("Failed to allocate memory.\n");
  }
  res->type = order->type;
  res->product = order->product;
  res->quantity = order->quantity;
  res->price = order->price;
  serialize_response(res, buf);
  for (int i = 0; i < num_traders; i++) {
    if (trader_id != traders[i].trader_id) {
      send_message(i, buf);
    }
  }
  // release reponse
  free(res);
}

void send_filled(int trader_id, int order_id, int quantity) {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  sprintf(buf, "%s %d %d;", MESSAGE_FILLED, order_id, quantity);
  send_message(trader_id, buf);
}

void send_invalid(int trader_id, int order_id) {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  sprintf(buf, "%s %d;", MESSAGE_INVALID, order_id);
  send_message(trader_id, buf);
}

void match_orders(Order* order) {
  if (num_orders == 0) {
    orderbook[num_orders] = *order;
    num_orders++;
    return;
  } else {
    // find the first order with the same product
    int same_product_index = -1;
    for (int i = 0; i < num_orders; i++) {
      if (strcmp(order->product.name, orderbook[i].product.name) == 0) {
        same_product_index = i;
        break;
      }
    }
    if (same_product_index == -1) {
      orderbook[num_orders] = *order;
      num_orders++;
      return;
    } else {
      if (order->type == BUY) {
        // find the all SELL orderes with the same product and price <= order->price
        OrderInfo orderInfo[MAX_ORDERS];
        int num_orderInfo = 0;
        for (int i = 0; i < num_orders; i++) {
          if (strcmp(order->product.name, orderbook[i].product.name) == 0 &&
              orderbook[i].price <= order->price && orderbook[i].type == SELL) {
            orderInfo[num_orderInfo].index = i;
            orderInfo[num_orderInfo].quantity = orderbook[i].quantity;
            orderInfo[num_orderInfo].price = orderbook[i].price;
            orderInfo[num_orderInfo].to_be_removed = 0;
            num_orderInfo++;
          }
        }
        if (num_orderInfo == 0) {
          orderbook[num_orders] = *order;
          num_orders++;
          return;
        } else {
          // sort orderInfo by price
          for (int i = 0; i < num_orderInfo - 1; i++) {
            for (int j = 0; j < num_orderInfo - i - 1; j++) {
              if (orderInfo[j].price < orderInfo[j + 1].price) {
                OrderInfo temp = orderInfo[j];
                orderInfo[j] = orderInfo[j + 1];
                orderInfo[j + 1] = temp;
              }
            }
          }
          // match orders
          for (int i = 0; i < num_orderInfo; i++) {
            int quantity_left = order->quantity - orderInfo[i].quantity;
            if (quantity_left >= 0) {
              // remove order from orderbook
              orderInfo[i].to_be_removed = 1;
              // update cash balance of seller
              int seller_trader_index = get_trader_by_id(orderbook[orderInfo[i].index].trader_id);
              traders[seller_trader_index].cash_balance +=
                  orderInfo[i].price * orderInfo[i].quantity;
              // charge 1% transaction fee to the trader who placed the order last
              int fee = (int)round(orderInfo[i].price * orderInfo[i].quantity * FEE_PERCENTAGE);
              exchange_fee_collected += fee;

              // update cache balance of buyer
              int buyer_trader_index = get_trader_by_id(order->trader_id);
              if (buyer_trader_index == -1) {
                perror("Error getting trader by id");
                exit(EXIT_FAILURE);
              }
              traders[buyer_trader_index].cash_balance -=
                  orderInfo[i].price * orderInfo[i].quantity;
              traders[buyer_trader_index].cash_balance -= fee;
              // update seller trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[seller_trader_index].positions[j].product.name) == 0) {
                  traders[seller_trader_index].positions[j].quantity -= orderInfo[i].quantity;
                  traders[seller_trader_index].positions[j].price +=
                      orderInfo[i].price * orderInfo[i].quantity;
                  break;
                }
              }
              // update buyer trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[buyer_trader_index].positions[j].product.name) == 0) {
                  traders[buyer_trader_index].positions[j].quantity += orderInfo[i].quantity;
                  traders[buyer_trader_index].positions[j].price -=
                      (orderInfo[i].price * orderInfo[i].quantity + fee);
                  break;
                }
              }
              send_filled(orderbook[orderInfo[i].index].trader_id,
                          orderbook[orderInfo[i].index].order_id, orderInfo[i].quantity);
              send_filled(order->trader_id, order->order_id, orderInfo[i].quantity);

              printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n",
                     LOG_EXCHANGE_PREFIX, orderbook[orderInfo[i].index].order_id,
                     orderbook[orderInfo[i].index].trader_id, order->order_id, order->trader_id,
                     orderInfo[i].price * orderInfo[i].quantity, fee);
              if (quantity_left == 0) {
                send_filled(orderbook[orderInfo[i].index].trader_id,
                            orderbook[orderInfo[i].index].order_id, orderInfo[i].quantity);
                send_filled(order->trader_id, order->order_id, orderInfo[i].quantity);
                break;
              }
              order->quantity = quantity_left;
            } else {
              // update orderbook
              orderInfo[i].to_be_removed = 0;
              orderbook[orderInfo[i].index].quantity -= order->quantity;
              // update cash balance of seller
              int seller_trader_index = get_trader_by_id(orderbook[orderInfo[i].index].trader_id);
              traders[seller_trader_index].cash_balance += orderInfo[i].price * order->quantity;
              // charge 1% transaction fee to the trader who placed the order last
              int fee = (int)round(orderInfo[i].price * order->quantity * FEE_PERCENTAGE);
              exchange_fee_collected += fee;
              // update cash balance of buyer
              int buyer_trader_index = get_trader_by_id(order->trader_id);
              if (buyer_trader_index == -1) {
                perror("Error getting trader by id");
                exit(EXIT_FAILURE);
              }
              traders[buyer_trader_index].cash_balance -= orderInfo[i].price * order->quantity;
              traders[buyer_trader_index].cash_balance -= fee;
              // update seller trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[seller_trader_index].positions[j].product.name) == 0) {
                  traders[seller_trader_index].positions[j].quantity -= order->quantity;
                  traders[seller_trader_index].positions[j].price +=
                      orderInfo[i].price * order->quantity;
                  break;
                }
              }
              // update buyer trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[buyer_trader_index].positions[j].product.name) == 0) {
                  traders[buyer_trader_index].positions[j].quantity += order->quantity;
                  traders[buyer_trader_index].positions[j].price -=
                      (orderInfo[i].price * order->quantity + fee);
                  break;
                }
              }
              printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n",
                     LOG_EXCHANGE_PREFIX, orderbook[orderInfo[i].index].order_id,
                     orderbook[orderInfo[i].index].trader_id, order->order_id, order->trader_id,
                     orderInfo[i].price * order->quantity, fee);
              send_filled(orderbook[orderInfo[i].index].trader_id,
                          orderbook[orderInfo[i].index].order_id, order->quantity);
              send_filled(order->trader_id, order->order_id, order->quantity);
              order->quantity = 0;
              break;
            }
          }
          // sort orderInfo by index
          for (int i = 0; i < num_orderInfo - 1; i++) {
            for (int j = 0; j < num_orderInfo - i - 1; j++) {
              if (orderInfo[j].index > orderInfo[j + 1].index) {
                OrderInfo temp = orderInfo[j];
                orderInfo[j] = orderInfo[j + 1];
                orderInfo[j + 1] = temp;
              }
            }
          }
          // reomve order from orderbook by index desc
          for (int i = num_orderInfo - 1; i >= 0; i--) {
            if (orderInfo[i].to_be_removed) {
              for (int j = orderInfo[i].index; j < num_orders - 1; j++) {
                orderbook[j] = orderbook[j + 1];
              }
              num_orders--;
            }
          }
          // check if order quantity is left
          if (order->quantity > 0) {
            orderbook[num_orders] = *order;
            num_orders++;
          }
        }
      } else if (order->type == SELL) {
        // find the all BUY orderes with the same product and price >= order->price
        OrderInfo orderInfo[MAX_ORDERS];
        int num_orderInfo = 0;
        for (int i = 0; i < num_orders; i++) {
          if (strcmp(order->product.name, orderbook[i].product.name) == 0 &&
              orderbook[i].price >= order->price && orderbook[i].type == BUY) {
            orderInfo[num_orderInfo].index = i;
            orderInfo[num_orderInfo].quantity = orderbook[i].quantity;
            orderInfo[num_orderInfo].price = orderbook[i].price;
            orderInfo[num_orderInfo].to_be_removed = 0;
            num_orderInfo++;
          }
        }
        if (num_orderInfo == 0) {
          orderbook[num_orders] = *order;
          num_orders++;
          return;
        } else {
          // sort orderInfo by price
          for (int i = 0; i < num_orderInfo - 1; i++) {
            for (int j = 0; j < num_orderInfo - i - 1; j++) {
              if (orderInfo[j].price < orderInfo[j + 1].price) {
                OrderInfo temp = orderInfo[j];
                orderInfo[j] = orderInfo[j + 1];
                orderInfo[j + 1] = temp;
              }
            }
          }
          // match orders
          for (int i = 0; i < num_orderInfo; i++) {
            int quantity_left = order->quantity - orderInfo[i].quantity;
            if (quantity_left >= 0) {
              // remove order from orderbook
              orderInfo[i].to_be_removed = 1;
              // update cash balance of seller
              int seller_trader_index = get_trader_by_id(order->trader_id);
              traders[seller_trader_index].cash_balance +=
                  orderInfo[i].price * orderInfo[i].quantity;
              // charge 1% transaction fee to the trader who placed the order last
              int fee = (int)round(orderInfo[i].price * orderInfo[i].quantity * FEE_PERCENTAGE);
              exchange_fee_collected += fee;
              // update cash balance of buyer
              int buyer_trader_index = get_trader_by_id(orderbook[orderInfo[i].index].trader_id);
              if (buyer_trader_index == -1) {
                perror("Error getting trader by id");
                exit(EXIT_FAILURE);
              }
              traders[buyer_trader_index].cash_balance -=
                  orderInfo[i].price * orderInfo[i].quantity;
              traders[buyer_trader_index].cash_balance -= fee;
              // update seller trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[seller_trader_index].positions[j].product.name) == 0) {
                  traders[seller_trader_index].positions[j].quantity -= orderInfo[i].quantity;
                  traders[seller_trader_index].positions[j].price +=
                      (orderInfo[i].price * orderInfo[i].quantity - fee);
                  break;
                }
              }
              // update buyer trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[buyer_trader_index].positions[j].product.name) == 0) {
                  traders[buyer_trader_index].positions[j].quantity += orderInfo[i].quantity;
                  traders[buyer_trader_index].positions[j].price -=
                      orderInfo[i].price * orderInfo[i].quantity;
                  break;
                }
              }
              send_filled(orderbook[orderInfo[i].index].trader_id,
                          orderbook[orderInfo[i].index].order_id, orderInfo[i].quantity);
              send_filled(order->trader_id, order->order_id, orderInfo[i].quantity);

              printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n",
                     LOG_EXCHANGE_PREFIX, orderbook[orderInfo[i].index].order_id,
                     orderbook[orderInfo[i].index].trader_id, order->order_id, order->trader_id,
                     orderInfo[i].price * orderInfo[i].quantity, fee);
              if (quantity_left == 0) {
                send_filled(orderbook[orderInfo[i].index].trader_id,
                            orderbook[orderInfo[i].index].order_id, orderInfo[i].quantity);
                send_filled(order->trader_id, order->order_id, orderInfo[i].quantity);
                break;
              }
              order->quantity = quantity_left;
            } else {
              // update orderbook
              orderInfo[i].to_be_removed = 0;
              orderbook[orderInfo[i].index].quantity -= order->quantity;
              // charge 1% transaction fee to the trader who placed the order last
              int fee = (int)round(orderInfo[i].price * order->quantity * FEE_PERCENTAGE);
              // update cash balance of seller
              int seller_trader_index = get_trader_by_id(order->trader_id);
              traders[seller_trader_index].cash_balance += orderInfo[i].price * order->quantity;
              traders[seller_trader_index].cash_balance -= fee;
              exchange_fee_collected += fee;
              // update cash balance of buyer
              int buyer_trader_index = get_trader_by_id(orderbook[orderInfo[i].index].trader_id);
              if (buyer_trader_index == -1) {
                perror("Error getting trader by id");
                exit(EXIT_FAILURE);
              }
              traders[buyer_trader_index].cash_balance -= orderInfo[i].price * order->quantity;
              // update seller trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[seller_trader_index].positions[j].product.name) == 0) {
                  traders[seller_trader_index].positions[j].quantity -= order->quantity;
                  traders[seller_trader_index].positions[j].price +=
                      (orderInfo[i].price * order->quantity - fee);
                  break;
                }
              }
              // update buyer trader positions
              for (int j = 0; j < num_products; j++) {
                if (strcmp(order->product.name,
                           traders[buyer_trader_index].positions[j].product.name) == 0) {
                  traders[buyer_trader_index].positions[j].quantity += order->quantity;
                  traders[buyer_trader_index].positions[j].price -=
                      orderInfo[i].price * order->quantity;
                  break;
                }
              }
              printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n",
                     LOG_EXCHANGE_PREFIX, orderbook[orderInfo[i].index].order_id,
                     orderbook[orderInfo[i].index].trader_id, order->order_id, order->trader_id,
                     orderInfo[i].price * order->quantity, fee);
              send_filled(orderbook[orderInfo[i].index].trader_id,
                          orderbook[orderInfo[i].index].order_id, order->quantity);
              send_filled(order->trader_id, order->order_id, order->quantity);
              order->quantity = 0;
              break;
            }
          }
          // sort orderInfo by index
          for (int i = 0; i < num_orderInfo - 1; i++) {
            for (int j = 0; j < num_orderInfo - i - 1; j++) {
              if (orderInfo[j].index > orderInfo[j + 1].index) {
                OrderInfo temp = orderInfo[j];
                orderInfo[j] = orderInfo[j + 1];
                orderInfo[j + 1] = temp;
              }
            }
          }
          // reomve order from orderbook by index desc
          for (int i = num_orderInfo - 1; i >= 0; i--) {
            if (orderInfo[i].to_be_removed) {
              for (int j = orderInfo[i].index; j < num_orders - 1; j++) {
                orderbook[j] = orderbook[j + 1];
              }
              num_orders--;
            }
          }
          // check if order quantity is left
          if (order->quantity > 0) {
            orderbook[num_orders] = *order;
            num_orders++;
          }
        }
      }
    }
  }
}

int validate_order(Order* order) {
  // check if product exists
  int product_exists = 0;
  for (int i = 0; i < num_products; i++) {
    if (strcmp(order->product.name, products[i].name) == 0) {
      product_exists = 1;
      break;
    }
  }
  if (!product_exists) {
    return -1;
  }
  // check if quanity is positive
  if (order->quantity <= 0) {
    return -1;
  }
  return 0;
}

void handle_order(char* buf, int trader_id) {
  Order* order = (Order*)malloc(sizeof(Order));
  deserialize_order(order, buf);
  order->trader_id = trader_id;
  int trader_index = get_trader_by_id(trader_id);
  if (trader_index == -1) {
    perror("Error getting trader by id");
    exit(EXIT_FAILURE);
  }
  // send ACCEPTED to trader
  accpeted(trader_id, order->order_id);
  // send MARKET to other traders
  market_message(trader_id, order);
  // match orders
  if (order->type == BUY || order->type == SELL) {
    // ivalid order handler
    if (validate_order(order) == -1) {
      send_invalid(trader_id, order->order_id);
    } else {
      match_orders(order);
    }
  }
  // release order
  free(order);
}

void handle_amend(char* buf, int trader_id) {}

void handle_cancel(char* buf, int trader_id) {}

void parsing_command(char* buf, int trader_id) {
  // strip ';' at the end
  char* pos = strchr(buf, ';');
  if (pos != NULL) {
    memset(pos, '\0', strlen(pos));
  }
  printf("%s [T%d] Parsing command: <%s>\n", LOG_EXCHANGE_PREFIX, trader_id, buf);
  // if buf start with 'BUY' or 'SELL', it is an order
  if (strncmp(buf, ORDER_TYPE_BUY, strlen(ORDER_TYPE_BUY)) == 0 ||
      strncmp(buf, ORDER_TYPE_SELL, strlen(ORDER_TYPE_SELL)) == 0) {
    handle_order(buf, trader_id);
  } else if (strncmp(buf, ORDER_TYPE_AMEND, strlen(ORDER_TYPE_AMEND)) == 0) {
    handle_amend(buf, trader_id);
  } else if (strncmp(buf, ORDER_TYPE_CANCEL, strlen(ORDER_TYPE_CANCEL)) == 0) {
    handle_cancel(buf, trader_id);
  } else {
    printf("%s Unknown command: <%s>\n", LOG_EXCHANGE_PREFIX, buf);
  }
  print_orderbook();
  print_position();
}

void sig_handler(int sig, siginfo_t* info, void* context) {
  if (sig == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    int trader_index = get_trader_by_pid(info->si_pid);
    if (trader_index == -1) {
      perror("Error getting trader by pid");
      exit(EXIT_FAILURE);
    }
    read(traders[trader_index].trader_fd, buf, sizeof(buf));
    parsing_command(buf, traders[trader_index].trader_id);
  }
  // else if (sig == SIGCHLD) {
  //   int trader_index = get_trader_by_pid(info->si_pid);
  //   printf("%s Trader %d disconnected\n", LOG_EXCHANGE_PREFIX, traders[trader_index].trader_id);
  //   fflush(stdout);
  // }
  // else {
  //   teardown();
  //   raise(sig);
  // }
}

void connect_to_pipes() {
  for (int i = 0; i < num_traders; i++) {
    traders[i].exchange_fd = open(traders[i].exchange_fifo, O_WRONLY);
    if (traders[i].exchange_fd == -1) {
      perror("Failed to open FIFO");
      exit(EXIT_FAILURE);
    }
    printf("%s Connected to %s\n", LOG_EXCHANGE_PREFIX, traders[i].exchange_fifo);
    traders[i].trader_fd = open(traders[i].trader_fifo, O_RDONLY);
    if (traders[i].trader_fd == -1) {
      perror("Failed to open FIFO");
      exit(EXIT_FAILURE);
    }
    printf("%s Connected to %s\n", LOG_EXCHANGE_PREFIX, traders[i].trader_fifo);
  }
}

void notify_market_open() {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  sprintf(buf, MESSAGE_MARKET_OPEN);
  for (int i = 0; i < num_traders; i++) {
    send_message(i, buf);
  }
}

int main(int argc, char** argv) {
  printf("%s Starting\n", LOG_EXCHANGE_PREFIX);
  if (argc > 1) {
    parse_args(argc, argv);
  }
  // set signal handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = sig_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("sigaction");
    return 1;
  }

  // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
  //   perror("sigaction");
  //   return 1;
  // }

  // fork child processes for traders
  fork_child_process();

  // check if all traders are connected
  // connect_to_pipes();

  // set MARKET OPEN to auto traders
  notify_market_open();

  // send MARKET SELL to auto traders
  // nofify_market_sell();

  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("epoll_create failed");
    exit(EXIT_FAILURE);
  }
  struct epoll_event ev[MAX_TRADERS];
  for (int i = 0; i < num_traders; i++) {
    ev[i].events = EPOLLIN | EPOLLHUP | EPOLLET;
    ev[i].data.fd = traders[i].trader_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, traders[i].trader_fd, &ev[i]) == -1) {
      perror("epoll_ctl failed");
      exit(EXIT_FAILURE);
    }
  }
  int all_fifo_closed = 0;
  // listen event of traders close fifo
  while (1) {
    // block signal
    // sigset_t block_mask;
    // sigemptyset(&block_mask);
    // sigaddset(&block_mask, SIGINT);
    // if (sigprocmask(SIG_BLOCK, &block_mask, NULL) == -1) {
    //   perror("sigprocmask failed");
    //   exit(EXIT_FAILURE);
    // }
    int nfds;
    if ((nfds = epoll_wait(epollfd, ev, num_traders, -1)) != -1) {
      for (int i = 0; i < nfds; i++) {
        if (ev[i].events & EPOLLHUP) {
          int trader_index = get_trader_by_fd(ev[i].data.fd);
          if (trader_index == -1) {
            perror("Error getting trader by fd");
            exit(EXIT_FAILURE);
          }
          printf("%s Trader %d disconnected\n", LOG_EXCHANGE_PREFIX,
                 traders[trader_index].trader_id);
          close(ev[i].data.fd);
          unlink(traders[trader_index].trader_fifo);
          // check all fifo closed
          all_fifo_closed = 1;
          for (int j = 0; j < num_traders; j++) {
            if (open(traders[j].trader_fifo, O_RDONLY) != -1) {
              all_fifo_closed = 0;
              break;
            }
          }
        }
      }
    }
    // unblock signal
    // if (sigprocmask(SIG_UNBLOCK, &block_mask, NULL) == -1) {
    //   perror("sigprocmask failed");
    //   exit(EXIT_FAILURE);
    // }
    // wait for all child processes to exit
    int status;
    pid_t wpid;
    int all_children_exited = 1;
    while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) {
      if (!WIFEXITED(status)) {
        all_children_exited = 0;
      }
    }
    if (all_children_exited && all_fifo_closed) {
      break;
    }
  }
  printf("%s Trading completed\n", LOG_EXCHANGE_PREFIX);
  printf("%s Exchange fees collected: $%d\n", LOG_EXCHANGE_PREFIX, exchange_fee_collected);
  teardown();
  return 0;
}