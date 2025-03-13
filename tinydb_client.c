#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 8079
#define BUFFER_SIZE 1024
#define HISTORY_FILE ".tinydb_history"
#define DEFAULT_TIMEOUT 5 // in seconds
#define MAX_RECONNECT_ATTEMPTS 3

int sock = -1;
int connected = 0;
char* history_file = NULL;
int response_timeout = DEFAULT_TIMEOUT;
int auto_reconnect = 0;
char last_host[256] = DEFAULT_HOST;
int last_port = DEFAULT_PORT;

void
cleanup(void);

void
handle_signal(int sig);

int
connect_to_server(const char* host, int port);

int
attempt_reconnect(void);

char*
send_command(const char* command, int is_reconnect_attempt);

int
execute_script(const char* filename);

void
print_help(void);

void
cleanup(void)
{
  if (sock >= 0) {
    close(sock);
  }

  if (history_file) {
    write_history(history_file);
    free(history_file);
  }
}

void
handle_signal(int sig)
{
  printf("\nExiting TinyDB client...\n");
  cleanup();
  exit(0);
}

int
connect_to_server(const char* host, int port)
{
  struct sockaddr_in server_addr;
  strncpy(last_host, host, sizeof(last_host) - 1);
  last_host[sizeof(last_host) - 1] = '\0';
  last_port = port;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    return -1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    close(sock);
    sock = -1;
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connection failed");
    close(sock);
    sock = -1;
    return -1;
  }

  return 0;
}

char*
send_command(const char* command, int is_reconnect_attempt)
{
  if (!connected) {
    return strdup("Not connected to server");
  }

  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  char flush_buffer[1024];
  while (recv(sock, flush_buffer, sizeof(flush_buffer), 0) > 0) {
    // we dont care
  }
  fcntl(sock, F_SETFL, flags);

  if (send(sock, command, strlen(command), 0) < 0) {
    printf("Error sending command: %s\n", strerror(errno));
    close(sock);
    sock = -1;
    connected = 0;

    if (!is_reconnect_attempt && attempt_reconnect() == 0) {
      return send_command(command, 1);
    }

    return strdup("Connection lost");
  }

  char buffer[4096] = { 0 };
  int bytes_received = 0;

  struct timeval tv;
  tv.tv_sec = response_timeout / 1000;
  tv.tv_usec = (response_timeout % 1000) * 1000;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    printf("Warning: Could not set socket timeout: %s\n", strerror(errno));
  }

  bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

  if (bytes_received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return strdup("Timeout waiting for response");
    } else {
      printf("Error receiving response: %s\n", strerror(errno));
      close(sock);
      sock = -1;
      connected = 0;
      if (!is_reconnect_attempt && attempt_reconnect() == 0) {
        return send_command(command, 1);
      }

      return strdup("Connection lost");
    }
  } else if (bytes_received == 0) {
    printf("Server closed the connection\n");
    close(sock);
    sock = -1;
    connected = 0;

    if (!is_reconnect_attempt && attempt_reconnect() == 0) {
      return send_command(command, 1);
    }

    return strdup("Connection lost");
  }

  buffer[bytes_received] = '\0';
  return strdup(buffer);
}

int
execute_script(const char* filename)
{
  FILE* file = fopen(filename, "r");
  if (!file) {
    printf("Error opening script file: %s\n", strerror(errno));
    return -1;
  }

  char line[1024];
  int line_number = 0;
  int success_count = 0;
  int error_count = 0;

  printf("Executing script: %s\n", filename);

  while (fgets(line, sizeof(line), file)) {
    line_number++;

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    if (strlen(line) == 0 || line[0] == '#') {
      continue;
    }

    if (strncmp(line, "SLEEP ", 6) == 0) {
      int sleep_time = atoi(line + 6);
      if (sleep_time > 0) {
        printf("Sleeping for %d seconds...\n", sleep_time);
        sleep(sleep_time);
      }
      continue;
    }

    printf("Line %d: %s\n", line_number, line);

    char* response = send_command(line, 0);
    printf("%s\n", response);

    if (strcmp(response, "Connection lost") == 0) {
      error_count++;
    } else {
      success_count++;
    }

    free(response);

    usleep(10000); // 10ms delay
  }

  fclose(file);

  printf("Script execution completed: %d successful, %d failed\n",
         success_count,
         error_count);

  return (error_count == 0) ? 0 : -1;
}

void
print_help()
{
  printf("\nTinyDB Client Commands:\n");
  printf("  .help                   - Show this help message\n");
  printf(
    "  .connect [host] [port]  - Connect to TinyDB server (default: %s:%d)\n",
    DEFAULT_HOST,
    DEFAULT_PORT);
  printf("  .disconnect             - Disconnect from server (for testing "
         "reconnection)\n");
  printf("  .reconnect              - Manually attempt to reconnect to last "
         "server\n");
  printf("  .exit                   - Exit the client\n");
  printf("  .clear                  - Clear the screen\n");
  printf("  .history                - Show command history\n");
  printf("  .source <filename>      - Execute commands from a script file\n");
  printf(
    "  .timeout <seconds>      - Set response timeout (default: %d seconds)\n",
    DEFAULT_TIMEOUT);
  printf("  .status                 - Show current connection status\n");
  printf("  .autoreconnect [on|off] - Enable/disable auto-reconnect (default: "
         "off)\n");
  printf(
    "\nAny other input will be sent as a command to the TinyDB server.\n\n");
}

int
attempt_reconnect(void)
{
  if (!auto_reconnect) {
    return -1;
  }

  printf("Connection lost. Attempting to reconnect...\n");

  for (int attempt = 1; attempt <= MAX_RECONNECT_ATTEMPTS; attempt++) {
    printf(
      "Reconnection attempt %d of %d...\n", attempt, MAX_RECONNECT_ATTEMPTS);

    if (connect_to_server(last_host, last_port) == 0) {
      printf("Reconnected to %s:%d\n", last_host, last_port);
      connected = 1;
      return 0;
    }

    sleep(1);
  }

  printf("Failed to reconnect after %d attempts\n", MAX_RECONNECT_ATTEMPTS);
  return -1;
}

int
main(int argc, char* argv[])
{
  char* host = DEFAULT_HOST;
  int port = DEFAULT_PORT;
  char* input;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  atexit(cleanup);

  char* home_dir = getenv("HOME");
  if (home_dir) {
    history_file = malloc(strlen(home_dir) + strlen(HISTORY_FILE) + 2);
    if (history_file) {
      sprintf(history_file, "%s/%s", home_dir, HISTORY_FILE);
      read_history(history_file);
    }
  }

  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    port = atoi(argv[2]);
  }

  printf("TinyDB Client\n");
  printf("Type .help for available commands\n");

  if (argc > 1) {
    printf("Connecting to %s:%d...\n", host, port);
    if (connect_to_server(host, port) == 0) {
      printf("Connected to TinyDB server at %s:%d\n", host, port);
      connected = 1;
    }
  }

  while ((input = readline("tinydb> ")) != NULL) {
    if (input[0] == '\0') {
      free(input);
      continue;
    }

    add_history(input);

    if (input[0] == '.') {
      if (strcmp(input, ".help") == 0) {
        print_help();
      } else if (strncmp(input, ".connect", 8) == 0) {
        char cmd_host[256] = DEFAULT_HOST;
        int cmd_port = DEFAULT_PORT;

        sscanf(input + 8, "%255s %d", cmd_host, &cmd_port);
        if (sock >= 0) {
          close(sock);
          sock = -1;
          connected = 0;
        }

        printf("Connecting to %s:%d...\n", cmd_host, cmd_port);
        if (connect_to_server(cmd_host, cmd_port) == 0) {
          printf("Connected to TinyDB server at %s:%d\n", cmd_host, cmd_port);
          connected = 1;
        }
      } else if (strcmp(input, ".disconnect") == 0) {
        if (connected) {
          printf("Manually disconnecting from server...\n");
          close(sock);
          sock = -1;
          connected = 0;
          printf("Disconnected. Auto-reconnect is %s.\n",
                 auto_reconnect ? "enabled" : "disabled");
        } else {
          printf("Not connected to any server.\n");
        }
      } else if (strcmp(input, ".reconnect") == 0) {
        if (!connected && strlen(last_host) > 0) {
          printf("Attempting to reconnect to %s:%d...\n", last_host, last_port);
          if (connect_to_server(last_host, last_port) == 0) {
            printf("Successfully reconnected to %s:%d\n", last_host, last_port);
            connected = 1;
          } else {
            printf("Failed to reconnect to %s:%d\n", last_host, last_port);
          }
        } else if (connected) {
          printf("Already connected to %s:%d\n", last_host, last_port);
        } else {
          printf("No previous connection details available. Use .connect "
                 "<host> <port> first.\n");
        }
      } else if (strcmp(input, ".exit") == 0) {
        printf("Exiting TinyDB client...\n");
        free(input);
        break;
      } else if (strcmp(input, ".clear") == 0) {
        printf("\033[2J\033[H"); // ANSI
      } else if (strcmp(input, ".history") == 0) {
        int i;
        HIST_ENTRY* entry;

        for (i = 0; i < history_length; i++) {
          if ((entry = history_get(i + 1)) != NULL) {
            printf("%5d  %s\n", i + 1, entry->line);
          }
        }
      } else if (strncmp(input, ".source", 7) == 0) {
        char filename[256] = "";
        sscanf(input + 7, "%255s", filename);

        if (strlen(filename) == 0) {
          printf("Usage: .source <filename>\n");
        } else if (!connected) {
          printf("Not connected to server. Use .connect to establish a "
                 "connection.\n");
        } else {
          execute_script(filename);
        }
      } else if (strncmp(input, ".timeout", 8) == 0) {
        int new_timeout = 0;
        if (sscanf(input + 8, "%d", &new_timeout) == 1 && new_timeout > 0) {
          response_timeout = new_timeout;
          printf("Response timeout set to %d seconds\n", response_timeout);
        } else {
          printf("Usage: .timeout <seconds> (must be a positive integer)\n");
          printf("Current timeout: %d seconds\n", response_timeout);
        }
      } else if (strcmp(input, ".status") == 0) {
        if (connected) {
          printf("Connected to server: %s:%d\n", last_host, last_port);
        } else {
          printf("Not connected to server\n");
        }
        printf("Response timeout: %d seconds\n", response_timeout);
        printf("Auto-reconnect: %s\n", auto_reconnect ? "enabled" : "disabled");
      } else if (strncmp(input, ".autoreconnect", 15) == 0) {
        char on_off[3];
        sscanf(input + 15, "%2s", on_off);
        if (strcmp(on_off, "on") == 0) {
          auto_reconnect = 1;
          printf("Auto-reconnect enabled\n");
        } else if (strcmp(on_off, "off") == 0) {
          auto_reconnect = 0;
          printf("Auto-reconnect disabled\n");
        } else {
          printf("Usage: .autoreconnect [on|off]\n");
        }
      } else {
        printf("Unknown command: %s\n", input);
        printf("Type .help for available commands\n");
      }
    } else {
      if (connected) {
        char* response = send_command(input, 0);
        printf("%s\n", response);
        free(response);
      } else {
        printf(
          "Not connected to server. Use .connect to establish a connection.\n");
      }
    }

    free(input);
  }

  return 0;
}