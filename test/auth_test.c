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
test_create_user()
{
  printf("Testing user creation...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "create_user test_user test_password");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Created user 'test_user': OK\n");

  response = send_command(sock, "create_user test_user another_password");
  assert(response != NULL);
  assert(strstr(response, "FAILED") != NULL);
  printf("  - Duplicate user creation rejected: OK\n");

  close(sock);
}

void
test_authenticate_user()
{
  printf("Testing user authentication...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "auth test_user test_password");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Correct credentials accepted: OK\n");

  response = send_command(sock, "auth test_user wrong_password");
  assert(response != NULL);
  assert(strstr(response, "FAILED") != NULL);
  printf("  - Incorrect password rejected: OK\n");

  response = send_command(sock, "auth nonexistent_user test_password");
  assert(response != NULL);
  assert(strstr(response, "FAILED") != NULL);
  printf("  - Non-existent user rejected: OK\n");

  close(sock);
}

void
test_delete_user()
{
  printf("Testing user deletion...\n");

  int sock = connect_to_server();
  assert(sock >= 0);

  char* response = send_command(sock, "auth default 123");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);

  response = send_command(sock, "delete_user test_user");
  assert(response != NULL);
  assert(strstr(response, "Ok") != NULL);
  printf("  - Deleted user 'test_user': OK\n");

  response = send_command(sock, "auth test_user test_password");
  assert(response != NULL);
  assert(strstr(response, "FAILED") != NULL);
  printf("  - Authentication with deleted user rejected: OK\n");

  response = send_command(sock, "delete_user default");
  assert(response != NULL);
  assert(strstr(response, "FAILED") != NULL);
  printf("  - Default user deletion rejected: OK\n");

  close(sock);
}

int
main()
{
  printf("TinyDB Authentication Tests\n");
  printf("==========================\n\n");

  test_create_user();
  printf("\n");

  test_authenticate_user();
  printf("\n");

  test_delete_user();
  printf("\n");

  printf("All authentication tests passed!\n");
  return 0;
}