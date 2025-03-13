#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 8079
#define DEFAULT_NUM_OPERATIONS 10000
#define BUFFER_SIZE 4096

typedef enum
{
  BENCHMARK_SET,
  BENCHMARK_GET,
  BENCHMARK_DEL,
  BENCHMARK_INCR,
  BENCHMARK_MIXED,
  BENCHMARK_COUNT
} BenchmarkType;

const char* benchmark_names[] = { "SET", "GET", "DEL", "INCR", "MIXED" };

volatile sig_atomic_t running = 1;
int operations_completed = 0;
double total_time = 0.0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void
handle_signal(int sig)
{
  running = 0;
}

int
connect_to_server(const char* host, int port)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket creation failed");
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    perror("invalid address");
    close(sock);
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("connection failed");
    close(sock);
    return -1;
  }

  return sock;
}

int
send_command(int sock, const char* command)
{
  char buffer[BUFFER_SIZE];

  if (send(sock, command, strlen(command), 0) < 0) {
    perror("send failed");
    return -1;
  }

  if (send(sock, "\n", 1, 0) < 0) {
    perror("send newline failed");
    return -1;
  }

  int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
  if (bytes_received < 0) {
    perror("recv failed");
    return -1;
  }

  buffer[bytes_received] = '\0';

  if (strstr(buffer, "ERROR") != NULL || strstr(buffer, "Invalid") != NULL) {
    return -1;
  }

  return 0;
}

void
generate_random_key(char* buffer, size_t size)
{
  const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  const size_t charset_size = sizeof(charset) - 1;

  for (size_t i = 0; i < size - 1; i++) {
    buffer[i] = charset[rand() % charset_size];
  }

  buffer[size - 1] = '\0';
}

void
generate_random_value(char* buffer, size_t size)
{
  const char charset[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const size_t charset_size = sizeof(charset) - 1;

  for (size_t i = 0; i < size - 1; i++) {
    buffer[i] = charset[rand() % charset_size];
  }

  buffer[size - 1] = '\0';
}

void
run_set_benchmark(int sock, int num_operations)
{
  char key[16];
  char value[32];
  char command[64];

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));
    generate_random_value(value, sizeof(value));

    snprintf(command, sizeof(command), "SET %s %s", key, value);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = send_command(sock, command);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    pthread_mutex_lock(&stats_mutex);
    operations_completed++;
    total_time += elapsed;
    pthread_mutex_unlock(&stats_mutex);

    if (result != 0) {
      fprintf(stderr, "Error executing command: %s\n", command);
    }
  }
}

void
run_get_benchmark(int sock, int num_operations)
{
  char key[16];
  char command[64];

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));
    char value[32];
    generate_random_value(value, sizeof(value));

    snprintf(command, sizeof(command), "SET %s %s", key, value);
    send_command(sock, command);
  }

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));

    snprintf(command, sizeof(command), "GET %s", key);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = send_command(sock, command);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    pthread_mutex_lock(&stats_mutex);
    operations_completed++;
    total_time += elapsed;
    pthread_mutex_unlock(&stats_mutex);

    if (result != 0) {
    }
  }
}

void
run_del_benchmark(int sock, int num_operations)
{
  char key[16];
  char command[64];

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));
    char value[32];
    generate_random_value(value, sizeof(value));

    snprintf(command, sizeof(command), "SET %s %s", key, value);
    send_command(sock, command);
  }

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));

    snprintf(command, sizeof(command), "DEL %s", key);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = send_command(sock, command);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    pthread_mutex_lock(&stats_mutex);
    operations_completed++;
    total_time += elapsed;
    pthread_mutex_unlock(&stats_mutex);

    if (result != 0) {
    }
  }
}

void
run_incr_benchmark(int sock, int num_operations)
{
  char key[16];
  char command[64];

  for (int i = 0; i < 100 && running; i++) {
    generate_random_key(key, sizeof(key));

    snprintf(command, sizeof(command), "SET %s 0", key);
    send_command(sock, command);
  }

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));

    snprintf(command, sizeof(command), "INCR %s", key);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = send_command(sock, command);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    pthread_mutex_lock(&stats_mutex);
    operations_completed++;
    total_time += elapsed;
    pthread_mutex_unlock(&stats_mutex);

    if (result != 0) {
    }
  }
}

void
run_mixed_benchmark(int sock, int num_operations)
{
  char key[16];
  char value[32];
  char command[64];

  for (int i = 0; i < num_operations && running; i++) {
    generate_random_key(key, sizeof(key));

    int op = rand() % 4;

    switch (op) {
      case 0: // SET
        generate_random_value(value, sizeof(value));
        snprintf(command, sizeof(command), "SET %s %s", key, value);
        break;
      case 1: // GET
        snprintf(command, sizeof(command), "GET %s", key);
        break;
      case 2: // DEL
        snprintf(command, sizeof(command), "DEL %s", key);
        break;
      case 3: // INCR
        snprintf(command, sizeof(command), "INCR %s", key);
        break;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = send_command(sock, command);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    pthread_mutex_lock(&stats_mutex);
    operations_completed++;
    total_time += elapsed;
    pthread_mutex_unlock(&stats_mutex);

    if (result != 0) {
    }
  }
}

void
run_benchmark(BenchmarkType type,
              const char* host,
              int port,
              int num_operations)
{
  int sock = connect_to_server(host, port);
  if (sock < 0) {
    fprintf(stderr, "Failed to connect to server\n");
    return;
  }

  printf("Connected to server, running %s benchmark...\n",
         benchmark_names[type]);

  operations_completed = 0;
  total_time = 0.0;

  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  switch (type) {
    case BENCHMARK_SET:
      run_set_benchmark(sock, num_operations);
      break;
    case BENCHMARK_GET:
      run_get_benchmark(sock, num_operations);
      break;
    case BENCHMARK_DEL:
      run_del_benchmark(sock, num_operations);
      break;
    case BENCHMARK_INCR:
      run_incr_benchmark(sock, num_operations);
      break;
    case BENCHMARK_MIXED:
      run_mixed_benchmark(sock, num_operations);
      break;
    default:
      fprintf(stderr, "Unknown benchmark type\n");
      close(sock);
      return;
  }

  clock_gettime(CLOCK_MONOTONIC, &end_time);

  double elapsed_seconds =
    (end_time.tv_sec - start_time.tv_sec) +
    (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

  printf("\nResults for %s benchmark:\n", benchmark_names[type]);
  printf("------------------\n");
  printf("Operations completed: %d\n", operations_completed);
  printf("Elapsed time: %.2f seconds\n", elapsed_seconds);
  printf("Operations per second: %.2f\n",
         operations_completed / elapsed_seconds);
  printf("Average operation time: %.6f seconds\n",
         total_time / operations_completed);
  printf("------------------\n");

  close(sock);
}

void
print_usage(const char* program_name)
{
  printf("Usage: %s [options]\n", program_name);
  printf("Options:\n");
  printf("  -h <host>       Server host (default: %s)\n", DEFAULT_HOST);
  printf("  -p <port>       Server port (default: %d)\n", DEFAULT_PORT);
  printf("  -n <operations> Number of operations (default: %d)\n",
         DEFAULT_NUM_OPERATIONS);
  printf("  -b <benchmark>  Benchmark type (default: all)\n");
  printf("                  0: SET\n");
  printf("                  1: GET\n");
  printf("                  2: DEL\n");
  printf("                  3: INCR\n");
  printf("                  4: MIXED\n");
  printf("  -?              Show this help message\n");
}

int
main(int argc, char* argv[])
{
  const char* host = DEFAULT_HOST;
  int port = DEFAULT_PORT;
  int num_operations = DEFAULT_NUM_OPERATIONS;
  int benchmark_type = -1; // -1 means run all benchmarks

  int opt;
  while ((opt = getopt(argc, argv, "h:p:n:b:?")) != -1) {
    switch (opt) {
      case 'h':
        host = optarg;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'n':
        num_operations = atoi(optarg);
        break;
      case 'b':
        benchmark_type = atoi(optarg);
        if (benchmark_type < 0 || benchmark_type >= BENCHMARK_COUNT) {
          fprintf(stderr, "Invalid benchmark type: %d\n", benchmark_type);
          print_usage(argv[0]);
          return 1;
        }
        break;
      case '?':
        print_usage(argv[0]);
        return 0;
      default:
        fprintf(stderr, "Unknown option: %c\n", opt);
        print_usage(argv[0]);
        return 1;
    }
  }

  if (num_operations <= 0) {
    fprintf(stderr, "Invalid number of operations: %d\n", num_operations);
    print_usage(argv[0]);
    return 1;
  }

  signal(SIGINT, handle_signal);

  srand(time(NULL));

  printf("TinyDB Benchmark\n");
  printf("------------------\n");
  printf("Server: %s:%d\n", host, port);
  printf("Operations: %d\n", num_operations);
  printf("------------------\n");

  if (benchmark_type == -1) {
    for (int i = 0; i < BENCHMARK_COUNT && running; i++) {
      run_benchmark(i, host, port, num_operations);
      printf("\n");
    }
  } else {
    run_benchmark(benchmark_type, host, port, num_operations);
  }

  return 0;
}