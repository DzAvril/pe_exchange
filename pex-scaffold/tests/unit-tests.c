#define _POSIX_C_SOURCE 199309L
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../pe_exchange_function.h"
#include "../pe_trader_function.h"
#include "cmocka.h"

extern Product products[MAX_PRODUCTS];
extern int num_products;
extern char* trader_path[MAX_TRADERS];
extern int num_traders;
extern Trader traders[MAX_TRADERS];

static void test_deserialize_response(void** state) {
  char buf[] = "MARKET BUY AAPL 100 200;";
  Response response;
  deserialize_response(buf, &response);
  assert_int_equal(response.type, BUY);
  assert_string_equal(response.product.name, "AAPL");
  assert_int_equal(response.quantity, 100);
  assert_int_equal(response.price, 200);
}

static void test_serialize_order(void** state) {
  Order* order = (Order*)malloc(sizeof(Order));
  order->type = BUY;
  order->order_id = 1;
  strcpy(order->product.name, "AAPL");
  order->quantity = 100;
  order->price = 200;

  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  serialize_order(order, buf);

  char expected_buf[] = "BUY 1 AAPL 100 200;";
  assert_string_equal(buf, expected_buf);
  free(order);
}

static void test_load_products(void** state) {
  // create /tmp/products.txt and write some products
  FILE* fp = fopen("/tmp/products.txt", "w");
  fprintf(fp, "4\n");
  fprintf(fp, "GPU\n");
  fprintf(fp, "CPU\n");
  fprintf(fp, "SWITCH\n");
  fprintf(fp, "TV\n");
  fclose(fp);
  load_products("/tmp/products.txt");
  assert_int_equal(num_products, 4);
  assert_string_equal(products[0].name, "GPU");
  assert_string_equal(products[1].name, "CPU");
  assert_string_equal(products[2].name, "SWITCH");
  assert_string_equal(products[3].name, "TV");
  unlink("/tmp/products.txt");
}

static void test_parse_args(void** state) {
  char* argv[] = {"./pe_exchange", "products.txt", "./pe_trader_0", "./pe_trader_1"};
  parse_args(4, argv);
  assert_int_equal(num_traders, 2);
  assert_string_equal(trader_path[0], "./pe_trader_0");
  assert_string_equal(trader_path[1], "./pe_trader_1");
}

static void test_deserialize_order(void** state) {
  char buf[] = "BUY 1 AAPL 100 200;";
  Order order;
  deserialize_order(&order, buf);
  assert_int_equal(order.type, BUY);
  assert_int_equal(order.order_id, 1);
  assert_string_equal(order.product.name, "AAPL");
  assert_int_equal(order.quantity, 100);
  assert_int_equal(order.price, 200);
}

static void test_trader_operator(void** state) {
  // reset traders
  memset(traders, 0, sizeof(Trader) * MAX_TRADERS);
  num_traders = 10;
  // create mock traders
  for (int i = 0; i < num_traders; i++) {
    traders[i].trader_id = i;
    traders[i].pid = i;
    traders[i].trader_fd = i;
  }
  int index = -1;
  for (int i = 0; i < num_traders; i++) {
    index = get_trader_by_id(traders[i].trader_id);
    assert_int_equal(index, i);
    index = get_trader_by_id(traders[i].pid);
    assert_int_equal(index, i);
    index = get_trader_by_id(traders[i].trader_fd);
    assert_int_equal(index, i);
  }
  index = get_trader_by_id(num_traders + 1);
  assert_int_equal(index, -1);
  index = get_trader_by_fd(num_traders + 1);
  assert_int_equal(index, -1);
  index = get_trader_by_pid(num_traders + 1);
  assert_int_equal(index, -1);
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
    char expected_buf[MAX_MESSAGE_LENGTH];
    memset(expected_buf, '\0', sizeof(expected_buf));
    sprintf(expected_buf, "SELL 0 AAPL 100 200;");
    assert_string_equal(buf, expected_buf);
  }
}

static void test_general(void** state) {
  // set handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = sig_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);

  // reset traders
  memset(traders, 0, sizeof(Trader) * MAX_TRADERS);
  num_traders = 1;
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
    int ret = mkfifo(traders[i].exchange_fifo, 0666);
    assert_int_equal(ret, 0);

    ret = mkfifo(traders[i].trader_fifo, 0666);
    assert_int_equal(ret, 0);

    // fork a child process to run pe_trader
    pid_t pid = fork();
    if (pid == 0) {
      // child process
      char trader_id[20];
      sprintf(trader_id, "%d", i);
      char* trader_argv[] = {"./pe_trader", trader_id, NULL};
      execv(trader_argv[0], trader_argv);
    } else if (pid > 0) {
      // parent process
      traders[i].trader_id = i;
      traders[i].pid = pid;
      traders[i].exchange_fd = open(traders[i].exchange_fifo, O_WRONLY);
      assert_int_not_equal(traders[i].exchange_fd, -1);

      traders[i].trader_fd = open(traders[i].trader_fifo, O_RDONLY);
      assert_int_not_equal(traders[i].trader_fd, -1);
      // send message to trader
      char buf[MAX_MESSAGE_LENGTH];
      memset(buf, '\0', sizeof(buf));
      sprintf(buf, "MARKET SELL AAPL 100 200;");
      send_message(i, buf);
    } else {
      perror("Error forking child process");
      exit(EXIT_FAILURE);
    }
  }
  int status;
  pid_t wpid;
  while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) {
  }
  // teardown
  teardown();
  for (int i = 0; i < num_traders; i++) {
    // assert trader fd is closed
    assert_int_equal(open(traders[i].trader_fifo, O_RDONLY), -1);
    // assert exchange fd is closed
    assert_int_equal(open(traders[i].exchange_fifo, O_WRONLY), -1);
  }
}

int main(int argc, char* argv[]) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_deserialize_response),
      cmocka_unit_test(test_serialize_order),
      cmocka_unit_test(test_load_products),
      cmocka_unit_test(test_parse_args),
      cmocka_unit_test(test_deserialize_order),
      cmocka_unit_test(test_trader_operator),
      cmocka_unit_test(test_general),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
