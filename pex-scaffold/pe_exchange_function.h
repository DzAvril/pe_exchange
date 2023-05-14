#include <math.h>
#include <sys/wait.h>
#include <time.h>

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

void load_products(const char* filename);
void parse_args(int argc, char** argv);
void fork_child_process();
void teardown();
void send_message(int trader_id, const char* message);
void deserialize_order(Order* order, char* buf);
int get_trader_by_id(int trader_id);
int get_trader_by_pid(int pid);
int get_trader_by_fd(int fd);
void print_report(Report* report);
void print_orderbook();
void print_position();
void accpeted(int trader_id, int order_id);
void serialize_response(Response* response, char* buf);
void market_message(int trader_id, Order* order);
void send_filled(int trader_id, int order_id, int quantity);
void send_invalid(int trader_id, int order_id);
void match_orders(Order* order);
int validate_order(Order* order);
void handle_order(char* buf, int trader_id);
void handle_amend(char* buf, int trader_id);
void handle_cancel(char* buf, int trader_id);
void parsing_command(char* buf, int trader_id);
void connect_to_pipes();
void notify_market_open();
void cleanup_trader(int trader_index);