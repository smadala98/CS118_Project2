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
  char payload[512];
};

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr, cli_addr;
  
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

  struct packet client_pkt;
  int len, n;
  n = recvfrom(sockfd, &client_pkt, sizeof(client_pkt), 0, (struct sockaddr *) &cli_addr, &len);
  printf("%d", client_pkt.seq_num);

  if (client_pkt.flags == (1 << 1)) {
    struct packet syn_ack_pkt;
    srand(time(0));
    syn_ack_pkt.seq_num = rand() % 25600;
    syn_ack_pkt.ack_num = client_pkt.seq_num + 1;
    syn_ack_pkt.flags = (1 << 1) + 1;
    if (sendto(sockfd, &syn_ack_pkt, sizeof(syn_ack_pkt), 0, (const struct sockaddr *) &cli_addr, 
	       sizeof(cli_addr)) < 0) {
      fprintf(stderr, "ERROR: Unable to send.");
      exit(1);
    }
  }

  struct packet new_pkt;
  n = recvfrom(sockfd, &new_pkt, sizeof(new_pkt), 0, (struct sockaddr *) &cli_addr, &len);
  printf("%s\n", new_pkt.payload);

  return 0;
}
