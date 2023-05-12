#include "pe_exchange.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "pe_common.h"
// Global variables
Product products[MAX_PRODUCTS];
int num_products = 0;
Trader traders[MAX_TRADERS];
int num_traders = MAX_TRADERS;

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
}

void parse_args(int argc, char** argv) {
  // printf("[debug] %s-%d Parsing args...\n", __FILE__, __LINE__);
  const char* filename = argv[1];
  // printf("[debug] %s-%d filename : %s.\n", __FILE__, __LINE__, filename);
  load_products(filename);
  num_traders = argc - 2;
}

void fork_child_process() {
  for (int i = 0; i < num_traders; i++) {
    sprintf(traders[i].exchange_fifo, FIFO_PATH_EXCHANGE_PREFIX, i);
    sprintf(traders[i].trader_fifo, FIFO_PATH_TRADER_PREFIX, i);
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

    if (mkfifo(traders[i].trader_fifo, 0666) < 0) {
      perror("Error creating FIFO");
      exit(EXIT_FAILURE);
    }

    // fork a child process to run pe_trader
    pid_t pid = fork();
    if (pid == 0) {
      // child process
      char trader_id[10];
      sprintf(trader_id, "%d", i);
      char* trader_argv[] = {"./pe_trader", trader_id, NULL};
      printf("[PEX-Milestone] Launching trader pe_trader\n");
      execv(trader_argv[0], trader_argv);
    } else if (pid > 0) {
      // parent process
      traders[i].trader_id = i;
      traders[i].pid = pid;
    } else {
      perror("Error forking child process");
      exit(EXIT_FAILURE);
    }
  }
}

void teardown() {
  printf("[PEX-Milestone] Trader disconnected\n");
  for (int i = 0; i < num_traders; i++) {
    close(traders[i].exchange_fd);
    close(traders[i].trader_fd);
    unlink(traders[i].exchange_fifo);
    unlink(traders[i].trader_fifo);
    remove(traders[i].trader_fifo);
    remove(traders[i].exchange_fifo);
  }
}

void sig_handler(int sig, siginfo_t *info, void *context) {
  if (sig == SIGUSR1) {
    // char buf[MAX_MESSAGE_LENGTH];
    // int trader_id = -1;
    // for (int i = 0; i < num_traders; i++) {
    //   if (traders[i].pid == info->si_pid) {
    //     trader_id = i;
    //     break;
    //   }
    // }
    // ssize_t len = read(traders[trader_id].trader_fd, buf, sizeof(buf));
    // printf("[debug] %s:%d Received message: %.*s\n\n", __FILE__, __LINE__, (int)len, buf);
  } else {
    teardown();
    raise(sig);
  }
}

void connect_to_pipes() {
  printf("[PEX-Milestone] Opened Name Pipes\n");
  for (int i = 0; i < num_traders; i++) {
    traders[i].exchange_fd = open(traders[i].exchange_fifo, O_WRONLY);
    if (traders[i].exchange_fd == -1) {
      perror("Failed to open FIFO");
      exit(EXIT_FAILURE);
    }
    traders[i].trader_fd = open(traders[i].trader_fifo, O_RDONLY);
    if (traders[i].trader_fd == -1) {
      perror("Failed to open FIFO");
      exit(EXIT_FAILURE);
    }
  }
}

void send_message(int trader_id, const char* message) {
  printf("[PEX-Milestone] Exchange -> Trader: %s\n", message);
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

void notify_market_open() {
  char buf[MAX_MESSAGE_LENGTH];
  memset(buf, '\0', sizeof(buf));
  sprintf(buf, MESSAGE_MARKET_OPEN);
  for (int i = 0; i < num_traders; i++) {
    send_message(i, buf);
  }
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
  // printf("Hello World from pe exchange!\n");
  if (argc > 1) {
    parse_args(argc, argv);
  }
  // set signal handler
  // signal(SIGUSR1, sig_handler);
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_sigaction = sig_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
      perror("sigaction");
      return 1;
  }

  // fork child processes for traders
  fork_child_process();

  // check if all traders are connected
  connect_to_pipes();

  // set MARKET OPEN to auto traders
  notify_market_open();

  // send MARKET SELL to auto traders
  nofify_market_sell();

  // wait for all child processes to exit
  int status;
  pid_t wpid;
  while ((wpid = wait(&status)) > 0) {
    if (WIFEXITED(status)) {
      printf("Child process %d terminated with exit status %d\n", wpid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("Child process %d terminated due to unhandled signal %d\n", wpid, WTERMSIG(status));
    }
  }
  teardown();
  return 0;
}