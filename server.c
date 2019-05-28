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

struct packet {
  int seq_num;
  int ack_num;
  int flags; // 1st bit is set for ACK, 2nd bit is set for SYN, 3rd bit is set for FIN
};
  
int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr, cli_addr;
  
  if (argc != 2) {
    fprintf(stderr, "ERROR: Incorrect number of arguments.");
    exit(1);
  }
  
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "ERROR: Unable to create socket file descriptor.");
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  memset(&cli_addr, 0, sizeof(cli_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERROR: Unable to bind socket.");
    exit(1);
  }

  srand(time(0));
  int conn_num = 0;
  int len, n;
  struct packet p;
  n = recvfrom(sockfd, &p, sizeof(p), 0, (struct sockaddr *) &cli_addr, &len);
  printf("%d", n);
   
  /*if (p.flags & 2) {
      conn_num++;
      struct packet syn_ack_p;
      syn_ack_p.ack_num = p.seq_num + 1;
      syn_ack_p.seq_num = rand() % 25600;
      syn_ack_p.flags = (1 << 1) + 1;
      if (sendto(sockfd, &syn_ack_p, sizeof(syn_ack_p), 0, (const struct sockaddr *) &cli_addr,
		 sizeof(cli_addr)) < 0) {
	fprintf(stderr, "ERROR: Unable to send.");
	exit(1);
      }
      }*/
}
