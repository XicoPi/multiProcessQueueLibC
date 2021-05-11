#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue_manager.h"


#define MAXMSG 2048


//static qmode_t qmode;
typedef struct {
  pid_t pid;
  int fd_pipe;
  bool status;
}proc_pair_t;

typedef struct {
  proc_pair_t child_pairs[MAXSUBPROC];
  int n;
  int iteration;
}child_list_t;

typedef struct {
  child_list_t child_set;
  qstatus_t state;
}queue_t;

static queue_t queues[MAXQUEUE];
//static char exec_path[100];

static void child_signal_handler(int signal)//si no fa l'esperat, sigaction MIRAR QUAN EL PROCES ES PARA
{
  pid_t pid;
  printf("handler\n");
  pid = wait(NULL);
  int handler;
  int i;
  handler = 0;
  i = 0;
  while (handler < MAXQUEUE) {
    handler++;
    i = 0;
    while (i < queues[handler].child_set.n && queues[handler].child_set.child_pairs[i].pid != pid) {
      i++;
    }
    if (i < queues[handler].child_set.n) {
      break;
    }
  }
  queues[handler].child_set.child_pairs[i].status = false;
  close(queues[handler].child_set.child_pairs[i].fd_pipe);
  printf("proc closed, pid: %d\n", queues[handler].child_set.child_pairs[i].pid);
}


static int create_child(proc_pair_t *pair, const char *exec_path)
{
  int fd_pipe[2], result;
  result = 0;
  if (pipe(fd_pipe) == -1) {
    return -1;
  }
  pid_t pid;
  pid = fork();
  if (pid == -1) {
    return -1;
  }
  else if (pid == 0) { //child proc
    close(STDIN_FILENO); //close stdin
    close(fd_pipe[1]); //close write pipe file
    dup(fd_pipe[0]); //duplicating the reading fd pipe to std input of process
    execlp(exec_path, "queueSubProc", NULL);
    exit(0);
  }
  else {
    close(fd_pipe[0]);
    pair->fd_pipe = fd_pipe[1];
    pair->pid = pid;
    pair->status = true;
  }
  return result;
}

void queue_init(void)
{
  static bool init = false;
  if (!init) {
    signal(SIGCHLD, child_signal_handler);
    //signal(SIGINT, close_all); // pel keyboard interrupt
    //signal(SIGTERM, close_all); //senyal de acabar
    int i;
    i = 0;
    while (i < MAXQUEUE) {
      queues[i].state = CLOSED;
      i++;
    }
    init = true;
  }
}

queue_handler_t queue_open(const char *queue_exec_path, int numProc)
{
  
  queue_handler_t result;
  result = 0;
  while (result < MAXQUEUE && queues[result].state != CLOSED) {
    result++;
  }
  if (result < MAXQUEUE) {
    
    queues[result].child_set.n = 0;
    queues[result].child_set.iteration = 0;
    proc_pair_t child_pair;
    while (queues[result].child_set.n < numProc) {
      create_child(&queues[result].child_set.child_pairs[queues[result].child_set.n], queue_exec_path);
      queues[result].child_set.n++;
    }
    queues[result].state = OPENED;
  }
  else {
    result = -1;
  }
  return result;
}

int queue_close(queue_handler_t handler)
{
  int result;
  result = 0;
  if (queues[handler].state == OPENED) {
    queues[handler].state = CLOSING;
    int i;
    i = 0;
    while (i < queues[handler].child_set.n) {
      if (queues[handler].child_set.child_pairs[i].status) {//only on signal handler case
	kill(queues[handler].child_set.child_pairs[i].pid,SIGTERM);
	//wait(NULL);
	//child_set.child_pairs[i].status = false;
	//close(child_set.child_pairs[i].fd_pipe);
	//printf("closed queue\n");
      }
      i++;
    }
    queues[handler].state = CLOSED;
  }
  else {
    result = -1;
  }
  
  return result;
}

qstatus_t queue_get_state(queue_handler_t handler)
{
  return queues[handler].state;
}

int queue_enqueue(queue_handler_t handler, const char *msg)
{
  int result;
  if (queues[handler].state == OPENED) {
    int i;
    size_t size;
    size = strlen(msg);
    i = 0;
    while (i < queues[handler].child_set.n && !queues[handler].child_set.child_pairs[queues[handler].child_set.iteration].status) {
      queues[handler].child_set.iteration = (queues[handler].child_set.iteration + 1) % queues[handler].child_set.n;
      i++;
    }
    if (i < queues[handler].child_set.n) {
      write(queues[handler].child_set.child_pairs[queues[handler].child_set.iteration].fd_pipe, msg, size);
      queues[handler].child_set.iteration = (queues[handler].child_set.iteration + 1) % queues[handler].child_set.n;
      printf("proc: %d\r\n", queues[handler].child_set.child_pairs[queues[handler].child_set.iteration].pid);
      //printf("proc: %d\r\n", queues[handler].child_set.iteration);
      result = 0;
    }
    else {//all child process had closed
      queues[handler].state = CLOSED;
      result = -1;
    }
  }
  else {
    result = -1;
  }
  return result;
}


