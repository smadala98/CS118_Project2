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
#include <errno.h>

struct packet {
  int seq_num;
  int ack_num;
  int flags; // If ACK 1st bit is set, SYN 2nd bit is set, FIN 3rd bit is set
  char payload[512];
  int FIN;
  int FIN_ACK;
};

struct packet* cwnd[512];
int cwnd_index = 0;
int end_index = 0;
double cwnd_size = 1;
double ssthresh = 10;

void slide_window(int ack_received) {
  for (int i = 0; i < cwnd_size; i++) {
    if (cwnd[cwnd_index]->seq_num < ack_received) {
      free(cwnd[cwnd_index]);
      cwnd_index++;
      i--;
    }
  }
}

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in serv_addr;
  int serv_addr_len = sizeof(serv_addr);

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

  // Get host address. TODO: If the argument given is IP address, need to add if statement to call gethostbyaddress.
  struct hostent* hp = gethostbyname(argv[1]);
  if (!hp) {
    fprintf(stderr, "ERROR: Host not reachable.");
    exit(1);
  }
  memcpy((void *)&serv_addr.sin_addr, hp->h_addr_list[0], hp->h_length);

  int cur_seq_num = 0;
  int cur_ack_num = 0;
  // Send the first syn packet as first part of three-way handshake.
  struct packet* syn_pkt = malloc(sizeof(struct packet));
  srand(time(0));  // Seeds based off current time.
  syn_pkt->seq_num = rand() % 25600;  // Guarantees that seq_num does not exceed 25600.
  cur_seq_num = syn_pkt->seq_num;
  syn_pkt->ack_num = 0;
  syn_pkt->flags = (1 << 1);  // Sets SYN flag.
  if (sendto(sockfd, syn_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
	     serv_addr_len) < 0) {
    fprintf(stderr, "ERROR: Unable to send.");
    exit(1);
  }

  int set_index = 0;
  while (!feof(fp)) {
    cwnd[cwnd_index + set_index] = malloc(sizeof(struct packet));
    memset(cwnd[cwnd_index + set_index]->payload, 0, 512);
    fread(cwnd[cwnd_index + set_index]->payload, 1, 512, fp);
    cwnd[cwnd_index + set_index]->seq_num = cur_seq_num + set_index + 1;
    set_index++;
  }
  end_index = cwnd_index + set_index;


  struct packet response_pkt;
  recvfrom(sockfd, &response_pkt, sizeof(response_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
  printf("%d, %d\n", response_pkt.seq_num, response_pkt.ack_num );
  // Check if three-way handshake has completed by check if SYN and ACK flags are set.
  if (response_pkt.flags == (1 << 1) + 1 && response_pkt.ack_num - 1 == syn_pkt->seq_num) {
    cur_ack_num = response_pkt.seq_num + 1;
    while (cwnd_index != end_index) {
      slide_window(response_pkt.ack_num);
      cwnd[cwnd_index]->ack_num = cur_ack_num;
      cwnd[cwnd_index]->flags = 1;
      if (sendto(sockfd, cwnd[cwnd_index], sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
		  serv_addr_len) < 0) {
	    fprintf(stderr, "ERROR: Unable to send.");
	    exit(1);
      }
      cwnd_index++;
    }
  }

  struct packet* fin_pkt = malloc(sizeof(struct packet));
  fin_pkt->seq_num = cur_ack_num + 524;
  fin_pkt->ack_num = cur_ack_num + 524;
  fin_pkt->flags = (1 << 1);
  fin_pkt->FIN=1;
  fin_pkt->FIN_ACK=0;
  if (sendto(sockfd, fin_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
	     serv_addr_len) < 0) {
    fprintf(stderr, "ERROR: Unable to send.");
    exit(1);
  }

  while(1){
    struct packet fin_ack_pkt;
    recvfrom(sockfd, &fin_ack_pkt, sizeof(fin_ack_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
    if (fin_ack_pkt.FIN == 1 && fin_ack_pkt.FIN_ACK == 1){
      struct packet* fin_pkt = malloc(sizeof(struct packet));
      fin_pkt->seq_num = cur_ack_num + 524;
      fin_pkt->ack_num = cur_ack_num + 524;
      fin_pkt->flags = (1 << 1);
      fin_pkt->FIN=1;
      fin_pkt->FIN_ACK=1;
      if (sendto(sockfd, fin_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
    	     serv_addr_len) < 0) {
        fprintf(stderr, "ERROR: Unable to send.");
        exit(1);
      }
      break;
    }
  }
  sleep(10);

  close(sockfd);
}
