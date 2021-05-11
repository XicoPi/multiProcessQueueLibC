#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

#include "sockUtils.h"


//int make_named_socket(const char *filename);
//Funció que genera una addreça pel socket a partir d'un hostname i un Port
void init_sockaddr(struct sockaddr_in *name, const char *hostname, uint16_t port) {

  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons(port);
  //htons() Assegura que la variable es guardi en Big endian en la memòria (format inet).
  if (strcmp(hostname, "") != 0) {
    hostinfo = gethostbyname(hostname);
    if (hostinfo == NULL) {
      fprintf(stderr, "Unknown host %s.\n", hostname);
      exit(EXIT_FAILURE);
    }
    name->sin_addr = *(struct in_addr *)  hostinfo->h_addr;
  }
  else {
    name->sin_addr.s_addr = htonl(INADDR_ANY);
  }
}

int makeServerSocket(uint16_t port) {

  int sockfd;
  struct sockaddr_in name; //declarem addr D'internet

  //Create a Socket
  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  
  init_sockaddr(&name, "", port);
  
  if (bind(sockfd, (struct sockaddr *) &name, sizeof(name)) > 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  return sockfd;
}
