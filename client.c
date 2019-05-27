#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr;

  if (argc != 4) {
    fprintf(stderr, "Incorrect amount of arguments.");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Unable to create socket.");
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));

  struct hostent* hp = gethostbyname(argv[1]);
  if (!hp) {
    fprintf(stderr, "Host not reachable.");
    exit(1);
  }
  memcpy((void *)&serv_addr.sin_addr, hp->h_addr_list[0], hp->h_length);

  char *hello = "Hello from client";
  if (sendto(sockfd, (const char *)hello, strlen(hello), 0, (const struct sockaddr *) &serv_addr, 
	     sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Unable to send.");
    exit(1);
  }
  close(sockfd);
}

