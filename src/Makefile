CC=gcc

sockUtils.o: sockUtils.h sockUtils.c
queue_manager.o: queue_manager.c queue_manager.h

shrtbl.o: shrtbl.h shrtbl.c

server.o: server.c sockUtils.h sockUtils.c
server: server.o sockUtils.o queue_manager.o #-lrt
queue.o: queue.c queue_manager.c queue_manager.h
queue: queue.o queue_manager.o
