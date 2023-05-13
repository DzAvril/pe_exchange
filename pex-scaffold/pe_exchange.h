#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"

typedef struct {
  int index;
  int price;
  int quantity;
  int to_be_removed;
  OrderType type;
} OrderInfo;

typedef struct {
  OrderType type;
  int quantity;
  int price;
  int num_order;
} OrderBrief;

typedef struct {
  Product product;
  int num_product;
  int buy_level;
  int sell_level;
  OrderBrief orderBrief[MAX_ORDERS];
} Report;
#endif
