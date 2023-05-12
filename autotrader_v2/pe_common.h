#ifndef PE_COMMON_H
#define PE_COMMON_H
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Constants
#define MAX_ORDERS 1000
#define MAX_ORDER_QUANTITY 1000
#define MAX_PRODUCTS 50
#define MAX_TRADERS 50
#define MAX_MESSAGE_LENGTH 512
#define MAX_TYPE_STRING_LENGTH 10
#define PRODUCT_NAME_LENGTH 17
#define ORDER_ID_RANGE 1000000
#define MAX_FIFO_NAME_LENGTH 32
#define TRANSACTION_FEE 0.01
#define FIFO_PATH_EXCHANGE_PREFIX "/tmp/pe_exchange_%d"
#define FIFO_PATH_TRADER_PREFIX "/tmp/pe_trader_%d"

#define ORDER_TYPE_BUY "BUY"
#define ORDER_TYPE_SELL "SELL"
#define ORDER_TYPE_AMEND "AMEND"
#define ORDER_TYPE_CANCEL "CANCEL"

#define MESSAGE_MARKET_OPEN "MARKET OPEN;"
#define RESPONSE_PREFIX "MARKET"
#define RESPONSE_LETTERS_NUM 5

typedef enum { BUY, SELL, AMEND, CANCEL } OrderType;

typedef struct {
  char name[PRODUCT_NAME_LENGTH];
} Product;

typedef struct {
  int order_id;
  Product product;
  int quantity;
  int price;
  OrderType type;
  // bool matched;
} Order;

typedef struct {
  int trader_id;
  pid_t pid;
  int cash_balance;
  int products_balance[MAX_PRODUCTS];
  Order orders[MAX_ORDERS];
  char exchange_fifo[MAX_FIFO_NAME_LENGTH];
  char trader_fifo[MAX_FIFO_NAME_LENGTH];
  int exchange_fd;
  int trader_fd;
} Trader;

typedef struct {
  OrderType type;
  Product product;
  int quantity;
  int price;
} Response;

#endif
