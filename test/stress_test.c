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
#define DEFAULT_NUM_CLIENTS 100
#define DEFAULT_NUM_QUERIES 1000
#define DEFAULT_QUERY_INTERVAL_MS 10
#define BUFFER_SIZE 4096

volatile sig_atomic_t running = 1;
int total_queries_sent = 0;
int total_queries_succeeded = 0;
int total_queries_failed = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
  int id;
  const char* host;
  int port;
  int num_queries;
  int query_interval_ms;
  int queries_sent;
  int queries_succeeded;
  int queries_failed;
} ClientParams;

const char* sample_queries[] = { "SET key1 value1",
                                 "GET key1",
                                 "SET key2 value2",
                                 "GET key2",
                                 "DEL key1",
                                 "DEL key2",
                                 "SET user:1 {\"name\":\"John\",\"age\":30}",
                                 "GET user:1",
                                 "SET user:2 {\"name\":\"Jane\",\"age\":25}",
                                 "GET user:2",
                                 "DEL user:1",
                                 "DEL user:2",
                                 "SET counter 0",
                                 "INCR counter",
                                 "GET counter",
                                 "DEL counter" };
const int num_sample_queries =
  sizeof(sample_queries) / sizeof(sample_queries[0]);

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
send_query(int sock, const char* query)
{
  char buffer[BUFFER_SIZE];

  if (send(sock, query, strlen(query), 0) < 0) {
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

void*
client_thread(void* arg)
{
  ClientParams* params = (ClientParams*)arg;
  int sock = -1;

  sock = connect_to_server(params->host, params->port);
  if (sock < 0) {
    fprintf(stderr, "Client %d: Failed to connect to server\n", params->id);
    return NULL;
  }

  printf("Client %d: Connected to server\n", params->id);

  for (int i = 0; i < params->num_queries && running; i++) {
    const char* query = sample_queries[rand() % num_sample_queries];

    int result = send_query(sock, query);

    params->queries_sent++;
    if (result == 0) {
      params->queries_succeeded++;
    } else {
      params->queries_failed++;
    }

    if (params->query_interval_ms > 0) {
      usleep(params->query_interval_ms * 1000);
    }
  }

  close(sock);

  printf("Client %d: Disconnected from server\n", params->id);

  pthread_mutex_lock(&stats_mutex);
  total_queries_sent += params->queries_sent;
  total_queries_succeeded += params->queries_succeeded;
  total_queries_failed += params->queries_failed;
  pthread_mutex_unlock(&stats_mutex);

  return NULL;
}

void
print_usage(const char* program_name)
{
  printf("Usage: %s [options]\n", program_name);
  printf("Options:\n");
  printf("  -h <host>       Server host (default: %s)\n", DEFAULT_HOST);
  printf("  -p <port>       Server port (default: %d)\n", DEFAULT_PORT);
  printf("  -c <clients>    Number of concurrent clients (default: %d)\n",
         DEFAULT_NUM_CLIENTS);
  printf("  -q <queries>    Number of queries per client (default: %d)\n",
         DEFAULT_NUM_QUERIES);
  printf("  -i <interval>   Interval between queries in ms (default: %d)\n",
         DEFAULT_QUERY_INTERVAL_MS);
  printf("  -?              Show this help message\n");
}

int
main(int argc, char* argv[])
{
  const char* host = DEFAULT_HOST;
  int port = DEFAULT_PORT;
  int num_clients = DEFAULT_NUM_CLIENTS;
  int num_queries = DEFAULT_NUM_QUERIES;
  int query_interval_ms = DEFAULT_QUERY_INTERVAL_MS;

  int opt;
  while ((opt = getopt(argc, argv, "h:p:c:q:i:?")) != -1) {
    switch (opt) {
      case 'h':
        host = optarg;
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 'c':
        num_clients = atoi(optarg);
        break;
      case 'q':
        num_queries = atoi(optarg);
        break;
      case 'i':
        query_interval_ms = atoi(optarg);
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

  if (num_clients <= 0 || num_queries <= 0 || query_interval_ms < 0) {
    fprintf(stderr, "Invalid parameters\n");
    print_usage(argv[0]);
    return 1;
  }

  signal(SIGINT, handle_signal);

  srand(time(NULL));

  printf("TinyDB Stress Test\n");
  printf("------------------\n");
  printf("Server: %s:%d\n", host, port);
  printf("Clients: %d\n", num_clients);
  printf("Queries per client: %d\n", num_queries);
  printf("Query interval: %d ms\n", query_interval_ms);
  printf("Total queries: %d\n", num_clients * num_queries);
  printf("------------------\n");

  pthread_t* threads = (pthread_t*)malloc(num_clients * sizeof(pthread_t));
  ClientParams* params =
    (ClientParams*)malloc(num_clients * sizeof(ClientParams));

  if (!threads || !params) {
    fprintf(stderr, "Memory allocation failed\n");
    free(threads);
    free(params);
    return 1;
  }

  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  for (int i = 0; i < num_clients; i++) {
    params[i].id = i + 1;
    params[i].host = host;
    params[i].port = port;
    params[i].num_queries = num_queries;
    params[i].query_interval_ms = query_interval_ms;
    params[i].queries_sent = 0;
    params[i].queries_succeeded = 0;
    params[i].queries_failed = 0;

    if (pthread_create(&threads[i], NULL, client_thread, &params[i]) != 0) {
      fprintf(stderr, "Failed to create thread for client %d\n", i + 1);
    }
  }

  for (int i = 0; i < num_clients; i++) {
    pthread_join(threads[i], NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &end_time);

  double elapsed_seconds =
    (end_time.tv_sec - start_time.tv_sec) +
    (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

  printf("\nResults:\n");
  printf("------------------\n");
  printf("Total queries sent: %d\n", total_queries_sent);
  printf("Successful queries: %d (%.2f%%)\n",
         total_queries_succeeded,
         (total_queries_sent > 0)
           ? (total_queries_succeeded * 100.0 / total_queries_sent)
           : 0.0);
  printf("Failed queries: %d (%.2f%%)\n",
         total_queries_failed,
         (total_queries_sent > 0)
           ? (total_queries_failed * 100.0 / total_queries_sent)
           : 0.0);
  printf("Elapsed time: %.2f seconds\n", elapsed_seconds);
  printf("Queries per second: %.2f\n", total_queries_sent / elapsed_seconds);
  printf("------------------\n");

  free(threads);
  free(params);

  return 0;
}