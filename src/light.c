// light.c
// Module which connects to a router and supplies lights.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSIZE 256
#define notok(x)  ((x) < 0)

void die(const char *msg) {
  perror(msg);
  exit(1);
}

void try(int ret, const char *msg) {
  if(notok(ret))
    die(msg);
}

int main(int argc, char *argv) {
  struct sockaddr_in servaddr;
  char buffer[BUFSIZE];
  int udpsock;
  struct hostent *host;

  try(udpsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP),
      "Failed to create udp socket");

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(13172);
  if(NULL == (host = gethostbyname("127.0.0.1"))) {
    fprintf(stderr, "not given a parseable IP address");
    exit(1);
  }

  bcopy(host->h_addr, &servaddr.sin_addr, host->h_length);

  printf("Sending packet\n");
  sprintf(buffer, "I'm a client");

  unsigned int servlen = sizeof(servaddr);

  sprintf(buffer, "This is a pocket\n");
  try(sendto(udpsock, buffer, BUFSIZE, 0,
  	     (struct sockaddr *) &servaddr, sizeof(servaddr)),
      "sendto()");

  try(recvfrom(udpsock, buffer, BUFSIZE, 0,
	       (struct sockaddr *) &servaddr, &servlen),
      "recvfrom()");
  printf("Got: %s\n", buffer);
  close(udpsock);
}
