#define _POSIX_C_SOURCE 199309L

#include "../../../pe_exchange.h"

// Global variables
extern Product products[MAX_PRODUCTS];
extern int num_products;
extern Trader traders[MAX_TRADERS];
extern int num_traders;
extern char* trader_path[MAX_TRADERS];
extern int exchange_fee_collected;
extern Order orderbook[MAX_ORDERS];
extern int num_orders;

void sig_handler(int sig, siginfo_t* info, void* context) {
  if (sig == SIGUSR1) {
    char buf[MAX_MESSAGE_LENGTH];
    int trader_index = get_trader_by_pid(info->si_pid);
    if (trader_index == -1) {
      perror("Error getting trader by pid");
      exit(EXIT_FAILURE);
    }
    read(traders[trader_index].trader_fd, buf, sizeof(buf));
    printf("%s Received message from trader: %s.\n", LOG_EXCHANGE_PREFIX, buf);
  }
}

void nofify_market_sell() {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  Response* res = (Response*)malloc(sizeof(Response));
  if (res == NULL) {
    perror("Failed to allocate memory.\n");
  }
  // randon create response
  srand(time(NULL));
  int random_product = rand() % num_products;
  res->type = SELL;
  res->product = products[random_product];
  res->quantity = rand() % MAX_ORDER_QUANTITY + 1;
  res->price = rand() % 1000;
  serialize_response(res, buf);
  // printf("[debug] %s-%d Send response: %s.\n", __FILE__, __LINE__, buf);
  for (int i = 0; i < num_traders; i++) {
    send_message(i, buf);
  }
  // release reponse
  free(res);
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
  nofify_market_sell();

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

          cleanup_trader(trader_index);
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