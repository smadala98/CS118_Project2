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
  short flags; // If ACK 1st bit is set, SYN 2nd bit is set, FIN 3rd bit is set
  short length;
  char payload[512];
};

struct packet* cwnd[51200000]; // make this larger enough to prevent bus error when transforming large files
int cwnd_index = 0;
int not_sent_index = 0;
int end_index = 0;
double cwnd_size = 1;
double cwnd_size_h = 0;
double ssthresh = 10;

clock_t time_pkt_sent_at;

void print_packet(struct packet pkt, int sent) {
  char type[16];
  memset(type, 0, 16);
  char buf[10];
  memset(buf, 0, 10);
  int added = 0;
  if (pkt.flags & 1) {
    strcpy(buf, "ACK");
    strcat(type, buf);
    added = 1;
  }
  if (pkt.flags & (1 << 1)) {
    if (added) {
      strcpy(buf, " SYN");
    } else {
      strcpy(buf, "SYN");
    }
    strcat(type, buf);
    added = 1;
  }
  if (pkt.flags & (1 << 2)) {
    if (added) {
      strcpy(buf, " FIN");
    } else {
      strcpy(buf, "FIN");
    }
    strcat(type, buf);
    added = 1;
  }

  if (sent) {
    printf("SEND %d 0 %.0f %.0f %s\n", pkt.seq_num, (cwnd_size + cwnd_size_h) * 512, ssthresh * 512, type);
  } else {
    printf("RECV %d %d %.0f %.0f %s\n", pkt.seq_num, pkt.ack_num, (cwnd_size + cwnd_size_h) * 512, ssthresh * 512, type);
  }
}

void slide_window(int ack_received) {
  int cwnd_size_increased = 0;
  for (int i = 0; i < cwnd_size; i++) {
    if (cwnd_index + i >= end_index)
      break;
    //    printf("ack: %d, seq_num: %d\n", ack_received, cwnd[i + cwnd_index]->seq_num);
    if (ack_received == cwnd[i + cwnd_index]->seq_num + cwnd[i + cwnd_index]->length) {
      if (!cwnd_size_increased && cwnd_size < 20) {
	cwnd_size_increased = 1;
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
      if (cwnd_size > 20)
	cwnd_size = 20;
      for (int j = 0; j < i; j++) {
	free(cwnd[cwnd_index]);
	cwnd_index++;
      }
    }
  }
  
  /*  for (int i = 0; i < cwnd_size; i++) {
    if (cwnd_index >= end_index)
      break;
    if (cwnd[cwnd_index]->seq_num < ack_received) {
      if (!cwnd_size_increased && cwnd_size < 20) {
	cwnd_size_increased = 1;
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
      if (cwnd_size > 20)
	cwnd_size = 20;
      free(cwnd[cwnd_index]);
      cwnd_index++;
      i--;
    }
    }*/
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
  cur_seq_num = syn_pkt->seq_num+1;
  syn_pkt->ack_num = 0;
  syn_pkt->flags = (1 << 1);  // Sets SYN flag.
  if (sendto(sockfd, syn_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
	     serv_addr_len) < 0) {
    fprintf(stderr, "ERROR: Unable to send handshake.");
    exit(1);
  }
  print_packet(*syn_pkt, 1);

  int set_index = 0;
  int buffer=0;
  while (!feof(fp)) {
    cwnd[cwnd_index + set_index] = malloc(sizeof(struct packet));
    memset(cwnd[cwnd_index + set_index]->payload, 0, 512);
    int bytes_read = fread(cwnd[cwnd_index + set_index]->payload, 1, 512, fp);
    //if (cur_seq_num + set_index*512 > 25600){
      //cwnd[cwnd_index + set_index]->seq_num = 0;
      //buffer=set_index;
      //cur_seq_num=0;
    //}else {
    cwnd[cwnd_index + set_index]->seq_num = cur_seq_num;
    cwnd[cwnd_index + set_index]->length = bytes_read;
    cur_seq_num += bytes_read;
    if (cur_seq_num > 25600) {
      cur_seq_num %= 25600;
    }
    //}
    set_index++;
  }
  end_index = cwnd_index + set_index;


  struct packet response_pkt;
  recvfrom(sockfd, &response_pkt, sizeof(response_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
  print_packet(response_pkt, 0);
  int final_seq_num = 0;
  // Check if three-way handshake has completed by check if SYN and ACK flags are set.
  if (response_pkt.flags == (1 << 1) + 1 && response_pkt.ack_num - 1 == syn_pkt->seq_num) {
    // Using not_sent_index prevents sending of duplicate packets, due to queuing of ACKs from server.
    while (cwnd_index != end_index) {
      for(; not_sent_index < cwnd_index + cwnd_size; not_sent_index++){
	if (not_sent_index == end_index)
	  break;

	if (not_sent_index == 0)
	  cwnd[not_sent_index]->ack_num = response_pkt.ack_num + 1;
	else
	  cwnd[not_sent_index]->ack_num = 0;
        cwnd[not_sent_index]->flags = 0;
	final_seq_num = cwnd[not_sent_index]->seq_num;
        if (sendto(sockfd, cwnd[not_sent_index], sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr, serv_addr_len) < 0) {
	  fprintf(stderr, "ERROR: Unable to send packet.");
	  exit(1);
        }
	print_packet(*cwnd[not_sent_index], 1);      
      }

      struct packet ack_response_pkt;
      recvfrom(sockfd, &ack_response_pkt, sizeof(ack_response_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
      print_packet(ack_response_pkt, 0);
      slide_window(ack_response_pkt.ack_num);
      if (ack_response_pkt.ack_num == cur_seq_num)
	break;
    }
  }

  struct packet* fin_pkt = malloc(sizeof(struct packet));
  fin_pkt->seq_num = cur_seq_num;
  fin_pkt->ack_num = 0;
  fin_pkt->flags = (1 << 2);

  if (sendto(sockfd, fin_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
	     serv_addr_len) < 0) {
    fprintf(stderr, "ERROR: Unable to send FIN");
    exit(1);
  }
  print_packet(*fin_pkt, 1);

  struct packet srv_fin_ack_pkt;
  /*  while(1) {
    struct timeval timeout = {0.5, 0};
    fd_set in_set;
    struct packet srv_fin_pkt;
    FD_ZERO(&in_set);
    FD_SET(sockfd, &in_set);
    int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);
    if(FD_ISSET(sockfd, &in_set)) {
      recvfrom(sockfd, &srv_fin_ack_pkt, sizeof(srv_fin_ack_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
      print_packet(srv_fin_ack_pkt, 0);
      if (srv_fin_ack_pkt.flags == 1 && srv_fin_ack_pkt.ack_num == fin_pkt->seq_num + 1) {
        break;
      }
    } else {
      if (sendto(sockfd, fin_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
		 serv_addr_len) < 0) {
        fprintf(stderr, "ERROR: Unable to send FIN");
        exit(1);
      }
    }
    }*/
  recvfrom(sockfd, &srv_fin_ack_pkt, sizeof(srv_fin_ack_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
  print_packet(srv_fin_ack_pkt, 0);
  if (srv_fin_ack_pkt.flags == 1 && srv_fin_ack_pkt.ack_num == fin_pkt->seq_num + 1) {

    while(1) {
      struct timeval timeout = {2, 0};
      fd_set in_set;
      struct packet srv_fin_pkt;
      FD_ZERO(&in_set);
      FD_SET(sockfd, &in_set);
      int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);
      if(FD_ISSET(sockfd, &in_set)){
	recvfrom(sockfd, &srv_fin_pkt, sizeof(srv_fin_pkt), 0, (struct sockaddr *) &serv_addr, &serv_addr_len);
	print_packet(srv_fin_pkt, 0);
      } else {
	break;
      }
      
      if (srv_fin_pkt.flags == (1 << 2)){
	struct packet* fin_ack_pkt = malloc(sizeof(struct packet));
	fin_ack_pkt->seq_num = fin_pkt->seq_num + 1;
	fin_ack_pkt->ack_num = srv_fin_pkt.seq_num + 1;
	fin_ack_pkt->flags = 1;
	if (sendto(sockfd, fin_ack_pkt, sizeof(struct packet), 0, (const struct sockaddr *) &serv_addr,
		   serv_addr_len) < 0) {
	  fprintf(stderr, "ERROR: Unable to send FIN ACK");
	  exit(1);
	}
	print_packet(*fin_ack_pkt, 1);
      }
    }
  }
  close(sockfd);
}
