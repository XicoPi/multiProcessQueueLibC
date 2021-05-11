#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "sockUtils.h"
#include "queue_manager.h"

#define PORT 8500
#define MAXMSG 2048
#define CONNUM 40 //nombre de connexions.
#define SERVER_TTL_SEG 15
#define NUM_QUEUE_SUBPROCCESS 2

//typedef enum {SOCK, PAIR, EMPTY}entry_type;

typedef struct {
  int con_fd;
  time_t action_time;
}connection_t;

static queue_handler_t pos_queue_h;


static int read_client_msg(int clientfd)
{
  char msg[MAXMSG];
  int msg_len, result;
  msg_len = read(clientfd, msg, MAXMSG);
  if (msg_len < 0) {
    perror("read error");
    exit(EXIT_FAILURE);
  }
  else if (msg_len == 0) { //EOF (connection closed by client)
    result = -1;
  }
  else {
    msg[msg_len] = '\0';
    fprintf(stdout, "Message received: %s", msg);
    if (queue_get_state(pos_queue_h) == OPENED && queue_enqueue(pos_queue_h,msg) == 0) {
    result = 0;
    }
    else {//queue closed
      result = -2;
    }
  }
  return result;
}


static int deleteArrayItem(connection_t *array, int elemIndex, int len)
{
  int i;
  i = elemIndex;
  len--;
  while (i < len) {
    array[i] = array[i+1];
    i++;
  }
  return len;
}

static void fd_list_close(connection_t *array, int len)
{
  int i;
  i = 0;
  while (i < len) {
    close(array[i].con_fd);
    i++;
  }
}

int main(void)
{

  //definint handlers del senyals del sistema
  
  
  int sockfd;

  char message[MAXMSG];
  fd_set active_fd_set, read_fd_set;
  struct sockaddr_in clientname;
  size_t size;
  int msg_len;
  //Creating the server socket
  sockfd = makeServerSocket((uint16_t)PORT);
  if (listen(sockfd, CONNUM) < 0) { //Preparat per escoltar el socket de comunicació
    perror("listen");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
  
  //inicialització el set de inputs pel select
  FD_ZERO(&active_fd_set); //reset del set de possibles inputs
  FD_SET(sockfd, &active_fd_set); //afegir el socket al set
  //inicialitzacio del set d'errors del set
  //afegint el stdin fd al FD_SET
  FD_SET(STDIN_FILENO, &active_fd_set);

  //QUEUE MANAGER INIT
  queue_init();
  pos_queue_h = queue_open("../orderProcessing/position.py", NUM_QUEUE_SUBPROCCESS);


  struct {
    connection_t filearray[FD_SETSIZE];
    int len;
  }fd_list;
  fd_list.len = 0;
  
  int i;
  connection_t new_con;
  time_t now_time;
  while (true) {
    read_fd_set = active_fd_set;

    if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) == -1) {
      if (errno == EINTR) {
	printf("system interrupt\n");
	continue;
      }
      perror("select");
      queue_close(pos_queue_h);
      close(sockfd);
      fd_list_close(fd_list.filearray, fd_list.len);
      exit(EXIT_FAILURE);
    }

    time(&now_time);

    if (FD_ISSET(sockfd, &read_fd_set)) { // new Connection request
      size = sizeof(clientname);
      int new_confd;
      new_confd = accept(sockfd,
			 (struct sockaddr *) &clientname,
			 (socklen_t *) &size);
      if (new_confd < 0) { //accept error
	perror("accept new connection");
	close(sockfd);
	queue_close(pos_queue_h);
	fd_list_close(fd_list.filearray, fd_list.len);
	exit(EXIT_FAILURE);
      }
      fprintf (stdout,
	       "Server: connect from host %s, port %u.\n",
	       inet_ntoa (clientname.sin_addr),
	       ntohs (clientname.sin_port));
      //adding the new connection to the list
      new_con.con_fd = new_confd;
      new_con.action_time = now_time;//adding the actual time
      fd_list.filearray[fd_list.len] = new_con;//new_confd;
      fd_list.len++;
      //adding the new connection to the active set
      FD_SET (new_confd, &active_fd_set);
    }

    if (FD_ISSET(STDIN_FILENO, &read_fd_set)) { //STDIN SELECTED
      if (read(STDIN_FILENO, message, MAXMSG) == 0) { //EOF
	queue_close(pos_queue_h);
	fd_list_close(fd_list.filearray, fd_list.len);
	close(sockfd);
	exit(EXIT_SUCCESS);
      }
    }
    
    i = 0;
    while (i < fd_list.len) {

      if (FD_ISSET(fd_list.filearray[i].con_fd,&read_fd_set)) { //connection communication selected
	//fprintf(stdout, "fd: %d", fd_list.filearray[i]);
	//Data arriving on an already-connected socket.
	fd_list.filearray[i].action_time = now_time;
	if (read_client_msg(fd_list.filearray[i].con_fd) == -1) {
	  close(fd_list.filearray[i].con_fd);
	  fprintf(stdout, "One connection closed\n");
	  FD_CLR(fd_list.filearray[i].con_fd, &active_fd_set); //clear connection from ACTIVE FD SET
	  fd_list.len = deleteArrayItem(fd_list.filearray, i, fd_list.len); //clear file from the list
	  }
	}
      else { //CONTROL DE TIMEOUT
	//printf("dif %ld\n", now_time - fd_list.filearray[i].action_time);
	if ((now_time - fd_list.filearray[i].action_time) > SERVER_TTL_SEG) {//si han passat TTL segons des de l'última interacció tanca la connexió
	  close(fd_list.filearray[i].con_fd);
	  fprintf(stdout, "One connection closed (timeout)\n");
	  FD_CLR(fd_list.filearray[i].con_fd, &active_fd_set); //clear connection from ACTIVE FD SET
	  fd_list.len = deleteArrayItem(fd_list.filearray, i, fd_list.len); //clear file from the list
	}
      }
      i++;
    }
  }

  close(sockfd);
  queue_close(pos_queue_h);
  return 0;
}
