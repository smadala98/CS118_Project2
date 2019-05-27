#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr, cli_addr;
  
  if (argc != 2) {
    fprintf(stderr, "Incorrect number of arguments.");
    exit(1);
  }
  
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Unable to create socket file descriptor.");
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  memset(&cli_addr, 0, sizeof(cli_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Unable to bind socket.");
    exit(1);
  }

  int len, n;
  char buffer[1024];
  n = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *) &cli_addr, &len);
  buffer[n] = '\0';
  printf("Client : %s\n", buffer);
  return 0;
}