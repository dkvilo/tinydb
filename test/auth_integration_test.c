#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8079
#define BUFFER_SIZE 4096

int
connect_to_server()
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    return -1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  if (inet_pton(AF_INET, SERVER_HOST, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    close(sock);
    return -1;
  }

  if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connection failed");
    close(sock);
    return -1;
  }

  return sock;
}

char*
send_command(int sock, const char* command)
{
  static char buffer[BUFFER_SIZE];

  if (send(sock, command, strlen(command), 0) < 0) {
    perror("Send failed");
    return NULL;
  }

  if (send(sock, "\n", 1, 0) < 0) {
    perror("Send newline failed");
    return NULL;
  }

  int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
  if (bytes_received < 0) {
    perror("Receive failed");
    return NULL;
  }

  buffer[bytes_received] = '\0';
  return buffer;
}

void
setup_test_users()
{
  printf("Setting up test users...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "auth default 123");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "create_user readonly_user password1");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Created read-only user: OK\n");

  response = send_command(sock, "create_user readwrite_user password2");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Created read-write user: OK\n");

  response = send_command(sock, "create_user admin_user password3");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Created admin user: OK\n");

  close(sock);
}

void
test_user_permissions()
{
  printf("Testing user permissions...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "auth default 123");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "set test_key test_value");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  close(sock);

  printf("  Testing read-only user...\n");
  sock = connect_to_server();
  assert(sock >= 0);

  response = send_command(sock, "auth readonly_user password1");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "get test_key");
  assert(response != NULL);
  assert(strstr(response, "test_value") != NULL);
  printf("    - Read operation: OK\n");

  response = send_command(sock, "set new_key new_value");
  printf("    - Write operation: %s\n",
         (strstr(response, "FAILED") != NULL) ? "Correctly denied"
                                              : "Incorrectly allowed");

  close(sock);

  printf("  Testing read-write user...\n");
  sock = connect_to_server();
  assert(sock >= 0);

  response = send_command(sock, "auth readwrite_user password2");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "get test_key");
  assert(response != NULL);
  assert(strstr(response, "test_value") != NULL);
  printf("    - Read operation: OK\n");

  response = send_command(sock, "set rw_key rw_value");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("    - Write operation: OK\n");

  close(sock);

  printf("  Testing admin user...\n");
  sock = connect_to_server();
  assert(sock >= 0);

  response = send_command(sock, "auth admin_user password3");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "get test_key");
  assert(response != NULL);
  assert(strstr(response, "test_value") != NULL);
  printf("    - Read operation: OK\n");

  response = send_command(sock, "set admin_key admin_value");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("    - Write operation: OK\n");

  response = send_command(sock, "del test_key");
  printf("    - Delete operation: %s\n",
         (strstr(response, "Ok") != NULL) ? "OK" : "Failed");

  close(sock);
}

void
cleanup_test_users()
{
  printf("Cleaning up test users...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "auth default 123");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "delete_user readonly_user");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Deleted read-only user: OK\n");

  response = send_command(sock, "delete_user readwrite_user");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Deleted read-write user: OK\n");

  response = send_command(sock, "delete_user admin_user");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Deleted admin user: OK\n");

  close(sock);
}

int
main()
{
  printf("TinyDB Authentication Integration Tests\n");
  printf("======================================\n\n");

  setup_test_users();
  printf("\n");

  test_user_permissions();
  printf("\n");

  cleanup_test_users();
  printf("\n");

  printf("All authentication integration tests completed!\n");
  return 0;
}