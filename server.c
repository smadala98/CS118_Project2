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
#include <errno.h>
#include <signal.h>
struct packet {
  int seq_num;
  int ack_num;
  int flags; // 1st bit is set for ACK, 2nd bit is set for SYN, 3rd bit is set for FIN
  char payload[512];
};
int conn_num = 0;
FILE* fp;

void sig_handler(int signo)
{
  if (signo == SIGINT){
    fclose(fp);
    exit(0);
  }

}

int main(int argc, char* argv[]) {

  if (signal(SIGINT, sig_handler) == SIG_ERR){
  }

  int sockfd;
  struct sockaddr_in serv_addr, cli_addr;
  int cli_addr_len = sizeof(cli_addr);
  srand(time(0));

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
  while(1){
    int cur_seq_num = 0;
    struct packet client_pkt;
    recvfrom(sockfd, &client_pkt, sizeof(client_pkt), 0, (struct sockaddr *) &cli_addr, &cli_addr_len);
    printf("%d, %d\n", client_pkt.seq_num, client_pkt.ack_num);

    // Check if SYN bit is set.
    if (client_pkt.flags == (1 << 1)) {
      conn_num++;
      struct packet syn_ack_pkt;
      syn_ack_pkt.seq_num = rand() % 25600;
      cur_seq_num = syn_ack_pkt.seq_num;
      syn_ack_pkt.ack_num = client_pkt.seq_num + 1;
      // Set SYN and ACK bit.
      syn_ack_pkt.flags = (1 << 1) + 1;
      if (sendto(sockfd, &syn_ack_pkt, sizeof(syn_ack_pkt), 0, (const struct sockaddr *) &cli_addr,
	         cli_addr_len) < 0) {
             fprintf(stderr, "ERROR: Unable to send.\n");
             exit(1);
          }
    }

    char filename[16];
    sprintf(filename, "%d.file", conn_num);
    fp = fopen(filename, "w+");

    // Need to put this in a loop to receive multiple packets from client.
    while(1) {
      struct packet new_pkt;
      recvfrom(sockfd, &new_pkt, sizeof(new_pkt), 0, (struct sockaddr *) &cli_addr, &cli_addr_len);

      // Check if received FIN bit.
      if (new_pkt.flags == (1 << 2)){
        struct packet fin_ack_pkt;
	      cur_seq_num++;
        fin_ack_pkt.seq_num = cur_seq_num;
        fin_ack_pkt.ack_num = new_pkt.seq_num + 1;
	// Set ACK for FIN packet sent from client.
        fin_ack_pkt.flags = 1;
        if (sendto(sockfd, &fin_ack_pkt, sizeof(fin_ack_pkt), 0, (const struct sockaddr *) &cli_addr, cli_addr_len) < 0) {
              fprintf(stderr, "ERROR: Unable to send.\n");
              exit(1);
        }
	// Create FIN packet for server to send to client.
	fin_ack_pkt.ack_num = 0;
	// Set FIN bit.
	fin_ack_pkt.flags = (1 << 2);
        if (sendto(sockfd, &fin_ack_pkt, sizeof(fin_ack_pkt), 0, (const struct sockaddr *) &cli_addr, cli_addr_len) < 0) {
              fprintf(stderr, "ERROR: Unable to send.\n");
              exit(1);
        }
	       break;
      }

      int payload_len=0;
      while(1) {
        if(new_pkt.payload[payload_len] == 0){
          break;
        }
        payload_len++;
      }
      fwrite(new_pkt.payload, sizeof(char), payload_len, fp);
      cur_seq_num++;
      // Send ACK packet for the client packet received to the client.
      struct packet ack_pkt;
      ack_pkt.seq_num = cur_seq_num;
      ack_pkt.ack_num = new_pkt.seq_num + 1;
      if (sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (const struct sockaddr *) &cli_addr, cli_addr_len) < 0) {
	     fprintf(stderr, "ERROR: Unable to send.\n");
	     exit(1);
      }
    }
    fclose(fp);
  }
}
