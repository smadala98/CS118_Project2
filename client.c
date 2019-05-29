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

  FILE* fp = fopen(argv[3], "r");
  if (!fp) {
    fprintf(stderr, "ERROR: Unable to open file.");
    exit(1);
  }

  // Create socket.
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "ERROR: Unable to create socket.");
    exit(1);
  }

  // Add socket port parameters.
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));

  // Get host address.
  struct hostent* hp = gethostbyname(argv[1]);
  if (!hp) {
    fprintf(stderr, "ERROR: Host not reachable.");
    exit(1);
  }
  memcpy((void *)&serv_addr.sin_addr, hp->h_addr_list[0], hp->h_length);

  // Send the first syn packet as first part of three-way handshake.
  struct packet syn_pkt;
  srand(time(0));  // Seeds based off current time.
  syn_pkt.seq_num = rand() % 25600;  // Guarantees that seq_num does not exceed 25600.
  syn_pkt.flags = (1 << 1);  // Sets SYN flag.
  if (sendto(sockfd, &syn_pkt, sizeof(syn_pkt), 0, (const struct sockaddr *) &serv_addr, 
	     sizeof(serv_addr)) < 0) {
    fprintf(stderr, "ERROR: Unable to send.");
    exit(1);
  }

  int len, n;
  struct packet response_pkt;
  n = recvfrom(sockfd, &response_pkt, sizeof(response_pkt), 0, (struct sockaddr *) &serv_addr, &len);
  // Check if three-way handshake has completed by check if SYN and ACK flags are set.
  if (response_pkt.flags == (1 << 1) + 1) {
    struct packet data_pkt;
    data_pkt.ack_num = response_pkt.seq_num + 1;
    data_pkt.seq_num = syn_pkt.seq_num + 1;
    data_pkt.flags = 1;
    int bytes_read = fread(data_pkt.payload, 1, 511, fp);
    data_pkt.payload[bytes_read] = 0;
    if (sendto(sockfd, &data_pkt, sizeof(data_pkt), 0, (const struct sockaddr *) &serv_addr, 
	       sizeof(serv_addr)) < 0) {
      fprintf(stderr, "ERROR: Unable to send.");
      exit(1);
    }
    
    //while (!feof(fp)) {
    //int num_chars = fread(
    //}
  }

  
  close(sockfd);
}

