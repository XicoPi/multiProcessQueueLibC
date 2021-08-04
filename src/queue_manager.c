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
#define READ 0
#define WRITE 1

//static qmode_t qmode;
typedef struct {
  pid_t pid;
  int read_fd_pipe;
  int write_fd_pipe;
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
  close(queues[handler].child_set.child_pairs[i].write_fd_pipe);
  close(queues[handler].child_set.child_pairs[i].read_fd_pipe);
  printf("proc closed, pid: %d\n", queues[handler].child_set.child_pairs[i].pid);
}


static int create_child(proc_pair_t *pair, const char *exec_path)
{
  int input_fd_pipe[2], output_fd_pipe[2], result;
  result = 0;
  if (pipe(input_fd_pipe) == -1) {
    return -1;
  }
  if (pipe(output_fd_pipe) == -1) {
    close(input_fd_pipe[0]);
    close(input_fd_pipe[1]);
    return -1;
  }
  pid_t pid;
  pid = fork();
  if (pid == -1) {
    return -1;
  }
  else if (pid == 0) { //child proc
    close(STDIN_FILENO); //close stdin
    close(STDOUT_FILENO); //close stdout
    close(input_fd_pipe[WRITE]); //close write pipe file
    close(output_fd_pipe[READ]); //close read pipe file
    if (dup2(input_fd_pipe[READ], STDIN_FILENO) == -1 || dup2(output_fd_pipe[WRITE], STDOUT_FILENO) == -1) {
      //duplicating the read/write fd pipe to std input/output of process
      close(input_fd_pipe[READ]); //close write pipe file
      close(output_fd_pipe[WRITE]); //close read pipe file
      exit(-1);
    }

    execlp(exec_path, "queueSubProc", NULL);
    exit(0);
  }
  else {
    close(input_fd_pipe[READ]);
    close(output_fd_pipe[WRITE]);
    pair->write_fd_pipe = input_fd_pipe[WRITE];
    pair->read_fd_pipe = output_fd_pipe[READ];
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
  
  queue_handler_t handler_result;
  handler_result = 0;
  while (handler_result < MAXQUEUE && queues[handler_result].state != CLOSED) {
    handler_result++;
  }
  if (handler_result < MAXQUEUE) {
    
    queues[handler_result].child_set.n = 0;
    queues[handler_result].child_set.iteration = 0;
    
    int i;
    i = 0;
    while (i < numProc) {
      create_child(&queues[handler_result].child_set.child_pairs[i], queue_exec_path);
      i++;
    }
    queues[handler_result].child_set.n = i;
    queues[handler_result].state = OPENED;
  }
  else {
    handler_result = -1;
  }
  return handler_result;
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
  child_list_t child_set;
  child_set = queues[handler].child_set;

  
  if (queues[handler].state == OPENED) {
    int i, iteration, n;
    i = 0;
    iteration = child_set.iteration;
    n = child_set.n;
    while (i < n && !child_set.child_pairs[iteration].status) {
      iteration = (iteration + 1) % n;
      i++;
    }
    if (i < child_set.n) {
      iteration = (iteration + 1) % n;
      child_set.iteration = iteration;
      queues[handler].child_set = child_set;

      int read_fd, write_fd;
      read_fd = child_set.child_pairs[child_set.iteration].read_fd_pipe;
      write_fd = child_set.child_pairs[child_set.iteration].write_fd_pipe;
      size_t size;
      size = strlen(msg);
      
      write(write_fd, msg, size);
      result = write_fd;

      printf("proc: %d\r\n", child_set.child_pairs[child_set.iteration].pid);
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


