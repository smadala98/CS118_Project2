#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

struct packet {
  int seq_num;
  int ack_num;
  int flags; // If ACK 1st bit is set, SYN 2nd bit is set, FIN 3rd bit is set
  char payload[512];
};

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr;

  if (argc != 4) {
    fprintf(stderr, "ERROR: Incorrect amount of arguments.");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "ERROR: Unable to create socket.");
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));

  struct hostent* hp = gethostbyname(argv[1]);
  if (!hp) {
    fprintf(stderr, "ERROR: Host not reachable.");
    exit(1);
  }
  memcpy((void *)&serv_addr.sin_addr, hp->h_addr_list[0], hp->h_length);

  struct packet first_packet;
  srand(time(0));
  first_packet.seq_num = rand() % 25600;
  first_packet.flags = (1 << 1);
  if (sendto(sockfd, &first_packet, sizeof(first_packet), 0, (const struct sockaddr *) &serv_addr, 
	     sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERROR: Unable to send.");
    exit(1);
  }
  int len, n;
  n = recvfrom(sockfd, &first_packet, sizeof(first_packet), 0, (struct sockaddr *) &serv_addr, &len);
  printf("%d", first_packet.ack_num);
  close(sockfd);
}

