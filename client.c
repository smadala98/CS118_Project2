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
#include <math.h>

struct packet {
  int seq_num;
  int ack_num;
  int flags; // If ACK 1st bit is set, SYN 2nd bit is set, FIN 3rd bit is set
  char payload[512];
};

struct packet* cwnd[51200000]; // make this larger enough to prevent bus error when transforming large files
int cwnd_index = 0;
int not_sent_index = 0;
int end_index = 0;
double cwnd_size = 1;
double cwnd_size_h = 0;
double ssthresh = 10;

void slide_window(int ack_received) {
  for (int i = 0; i < cwnd_size; i++) {
    if (cwnd_index >= end_index)
      break;
    if (cwnd[cwnd_index]->seq_num < ack_received) {
      if (cwnd[cwnd_index]->seq_num == ack_received - 1) {
	// Increments cwnd.
	if (cwnd_size < ssthresh) {
	  cwnd_size++;
	} else {
	  cwnd_size_h += (double)1/cwnd_size;
	  // Have to compare this way due to floating point error.
	  if (fabs(cwnd_size_h - 1) < 0.0001) {
	    cwnd_size++;
	    cwnd_size_h = 0;
	  }
	}
      }
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
    fprintf(stderr, "ERROR: Unable to send handshake.");
    exit(1);
  }

  int    set_index = 0;
  while (!feof(fp)) {
    cwnd[cwnd_index + set_index] = malloc(sizeof(struct packet));
    memset(cwnd[cwnd_index + set_index]->payload, 0, 512);
    fread(cwnd[cwnd_index + set_index]->payload, 1, 511, fp);
    cwnd[cwnd_index + set_index]->seq_num = cur_seq_num + set_index + 1;
    set_index++;
  }
  end_index = cwnd_index + set_index;


  struct packet response_pkt;
  recvfrom(sockfd, &response_pkt, sizeof(response_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
  printf("%d, %d\n", response_pkt.seq_num, response_pkt.ack_num );
  int final_seq_num = 0;
  // Check if three-way handshake has completed by check if SYN and ACK flags are set.
  if (response_pkt.flags == (1 << 1) + 1 && response_pkt.ack_num - 1 == syn_pkt->seq_num) {
    cur_ack_num = response_pkt.seq_num + 1;
    //    int doneseq=0;
    //    int dupacks=0;
    // Using not_sent_index prevents sending of duplicate packets, due to queuing of ACKs from server.
    while (cwnd_index != end_index) {
      for(; not_sent_index < cwnd_index + cwnd_size; not_sent_index++){
	      if (not_sent_index == end_index)
	      break;

        cwnd[not_sent_index]->ack_num = cur_ack_num;
        cwnd[not_sent_index]->flags = 1;
	      final_seq_num = cwnd[not_sent_index]->seq_num;
        if (sendto(sockfd, cwnd[not_sent_index], sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr, serv_addr_len) < 0) {
  	    fprintf(stderr, "ERROR: Unable to send packet.");
  	    exit(1);
        }
      }

      struct packet ack_response_pkt;
      recvfrom(sockfd, &ack_response_pkt, sizeof(ack_response_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
      printf("%d, %d", ack_response_pkt.seq_num, ack_response_pkt.ack_num );
      slide_window(ack_response_pkt.ack_num);

      /*if (ack_response_pkt.ack_num == doneseq+1){
         doneseq++;
         if (cwnd_size <= ssthresh){
           cwnd_size++;
         }else{
           cwnd_size_h+=(double)(1/cwnd_size);
           if (cwnd_size_h==1){
             cwnd_size++;
             cwnd_size_h=0;
           }
         }
      } else if (ack_response_pkt.ack_num == doneseq){
        dupacks++;
        if (dupacks == 2){
          ssthresh=(int)(ssthresh/2);
          cwnd[doneseq+1]->ack_num = doneseq+1;
          cwnd[doneseq+1]->flags = 1;
          if (sendto(sockfd, cwnd[doneseq+1], sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr, serv_addr_len) < 0) {
    	       fprintf(stderr, "ERROR: Unable to send packet.");
    	       exit(1);
          }
        }
	}*/
      printf(" cwnd = %f, cwnd_size_h = %f\n", cwnd_size, cwnd_size_h);
    }
  }

  struct packet* fin_pkt = malloc(sizeof(struct packet));
  cur_seq_num++;
  fin_pkt->seq_num = final_seq_num;
  fin_pkt->ack_num = 0;
  fin_pkt->flags = (1 << 2);

  if (sendto(sockfd, fin_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
	     serv_addr_len) < 0) {
    fprintf(stderr, "ERROR: Unable to send FIN");
    exit(1);
  }

  struct packet srv_fin_ack_pkt;
  recvfrom(sockfd, &srv_fin_ack_pkt, sizeof(srv_fin_ack_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
  if (srv_fin_ack_pkt.flags == 1 && srv_fin_ack_pkt.ack_num == fin_pkt->seq_num + 1) {
    time_t cur_time = time(NULL);
    while(time(NULL) - cur_time < 2){
      struct timeval timeout = {2, 0};
      fd_set in_set;
      struct packet srv_fin_pkt;
      FD_ZERO(&in_set);
      FD_SET(sockfd, &in_set);
      int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);
      if(FD_ISSET(sockfd, &in_set)){
        recvfrom(sockfd, &srv_fin_pkt, sizeof(srv_fin_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
      } else {
        break;
      }

      if (srv_fin_pkt.flags == (1 << 2)){
	       struct packet* fin_ack_pkt = malloc(sizeof(struct packet));
	       fin_ack_pkt->seq_num = fin_pkt->seq_num + 1;
	       printf("%d,", fin_ack_pkt->seq_num);
	       fin_ack_pkt->ack_num = srv_fin_pkt.seq_num + 1;
	       printf(" %d\n", fin_ack_pkt->ack_num);
	       fin_ack_pkt->flags = 1;
	       if (sendto(sockfd, fin_ack_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
		       serv_addr_len) < 0) {
	            fprintf(stderr, "ERROR: Unable to send FIN ACK");
	            exit(1);
	       }
         break;
      }
    }
  }

  close(sockfd);
}
