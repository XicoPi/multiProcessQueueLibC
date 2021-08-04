#ifndef _MASTER_QUEUE_H_
#define _MASTER_QUEUE_H_

#define MAXSUBPROC 16//per queue
#define MAXQUEUE 10
typedef enum {OPENED, CLOSED, CLOSING}qstatus_t;

//typedef enum {READ, WRITE}qmode_t;

typedef int queue_handler_t;

void queue_init(void);
queue_handler_t queue_open(const char *queue_exec_path, int numProc);
/*
el programa que executarà el mòdul, rep els missatges per la seva entrada estandar (stdin) 
*/
int queue_close(queue_handler_t handler);
qstatus_t queue_get_state(queue_handler_t handler);
int queue_enqueue(queue_handler_t handler, const char *msg);
//returns queue response read_fd
//if there are more active process than numProc it returns -1


#endif
