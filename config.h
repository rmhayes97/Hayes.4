#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <time.h>



/* Structure for times */
struct times {
	int ns;
	int s;
};

/* Structure for process control block */
struct PCB {
	int pid;
	int lpid;
	int pClass;
	int precedence;

	struct times cpuTime;
	struct times systemTime;
	struct times timeBlocked;
};

/* Structure for shared resources */
struct sharedRes {
	struct PCB pcbTable[18];
	struct times time;
};

/* Structure for queue */
struct Queue {
	int maxVolume;
	int head;
	int tail; 
	int *qArr;
	int size;
};

/* Structure for message */
struct {
	long msgType;
	char msg[100];
} msg;


void killOSS(int);
void cleanProgram();
void incrementTime(struct times*, int, int);
int blockEmpty();
void ossScheduler();

struct Queue* buildQ(int);
int qFull(struct Queue*);
int qEmpty(struct Queue*);
int qLength(struct Queue*);
void enqueue(struct Queue*,int);
int dequeue(struct Queue*);


#endif
