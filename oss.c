
#include "config.h"

FILE* filePointer;

const int baseTimeQuantum = 10;
const int maxTimeBetweenNewProcSec = 1; 
const int maxTimeBetweenNewProcNS = 500;
const int maxLogLines = 10000;
const int realTime = 15;

int SHMid = 0;
int atKid = 0;
int atOSS = 0;
int setProcesses = 0;
int usedPIDs[20];
int printedTime = 0;

struct PCB *controlTable;
struct sharedRes *resPTR;
struct times eNum = {0,0};
struct times cpuTime;
struct times totalBlocked;
struct times waitTime;
struct times idleTime;

/* empties the contents of the blocks */
int blockEmpty() {

	for(int i = 0; i < 20; i++) {
		if(usedPIDs[i] == 0) {

			usedPIDs[i] = 1;

			return i;
		}
	}
	return -1;
}

/* deallocates memory and cleans the message */
void cleanProgram() {
	fclose(filePointer);

	msgctl(atKid,IPC_RMID,NULL);
	msgctl(atOSS,IPC_RMID,NULL);

	shmdt((void*)resPTR);	
	shmctl(SHMid, IPC_RMID, NULL);
}

/* kills the program */
void killOSS(int sig) {
	switch(sig) {
		case SIGINT:
			printf("\nProgram caught 'CTRL+C', exiting.\n");
			break;
	}
	cleanProgram();
	kill(0,SIGKILL);
}

/* used for keeping track of time running */
void incrementTime(struct times* time, int sec, int ns) {
	time->ns += ns;
	time->s += sec;

	while(time->ns >= 1000000000) {
		time->s++;
		time->ns -=1000000000;
	}
}

/* got Queue structure functions from GeeksForGeeks */
struct Queue* buildQ(int max) {

	struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));	
	
	queue->head = queue->size = 0;
	queue->tail = max - 1;
	queue->maxVolume = max;
	queue->qArr = (int*)malloc(queue->maxVolume*sizeof(int));

	return queue;
}

int qFull(struct Queue* queue) {
	if (queue->size != queue->maxVolume){
		return 0;
	} else {
		return 1;
	}
}


int qEmpty(struct Queue* queue) {
	return(queue->size == 0);
}

void enqueue(struct Queue* queue, int id) {
	if(qFull(queue) == 1){
		return;
	}
	
	queue->size = queue->size + 1;
	queue->tail = (queue->tail + 1)%queue->maxVolume;
	queue->qArr[queue->tail] = id;

}

int dequeue(struct Queue* queue){

	if(qEmpty(queue)){
		return INT_MIN;
	}

	int id = queue->qArr[queue->head];
	queue->size = queue->size - 1;
	queue->head = (queue->head + 1) % queue->maxVolume;

	return id;
}

/* schedules the processes */
void ossScheduler() {
	srand(time(0));
	pid_t childPID;
	int isGoing = 0;
	int logLines = 0;
	int pStatus = 0;

	int maxGenerated = 50;
	int pGenCounter = 0;
	int goingProcs = 0;
	int nowActive = 0;

	/* time quantums for the different queues */
	int q1Quant = baseTimeQuantum  * 100000;
	int q2Quant = baseTimeQuantum * 2 * 100000;
	int q3Quant = baseTimeQuantum * 3 * 100000;
	int q4Quant = baseTimeQuantum * 4 * 100000; 


	/* queues made for storing pids */
	struct Queue* q1 = buildQ(setProcesses);
	struct Queue* q2 = buildQ(setProcesses);
	struct Queue* q3 = buildQ(setProcesses);
	struct Queue* q4 = buildQ(setProcesses);
	struct Queue* blockedProcQ = buildQ(setProcesses);

	
	/* counter for the number of terminated processes */
	while(pGenCounter <= 50) {

		incrementTime(&(resPTR->time), 0, 100000);

		if (isGoing == 0) {
			incrementTime(&(idleTime),0,100000);
		}

		if(  maxGenerated > 0 &&  (goingProcs < setProcesses)  &&  ((resPTR->time.s > eNum.s)  ||  (resPTR->time.s == eNum.s && resPTR->time.ns >= eNum.ns))  ) {

			int secs = (rand() % (maxTimeBetweenNewProcSec + 1));
			int ns = (rand() % (maxTimeBetweenNewProcNS + 1));

			eNum.s = resPTR->time.s;
			eNum.ns = resPTR->time.ns;

			incrementTime(&eNum,secs,ns);
			int freshProcess = blockEmpty();

			if (freshProcess > -1) {

				if ((childPID = fork()) == 0) {

					char str[10];
					sprintf(str, "%d", freshProcess);
					execl("./user",str,NULL);
					exit(0);

				}	

				resPTR->pcbTable[freshProcess].systemTime.s = resPTR->time.s;
				resPTR->pcbTable[freshProcess].systemTime.ns = resPTR->time.ns;	

				resPTR->pcbTable[freshProcess].pid = childPID;
				resPTR->pcbTable[freshProcess].lpid = freshProcess;
				resPTR->pcbTable[freshProcess].pClass = ((rand()% 100) < realTime) ? 2 : 1;

				resPTR->pcbTable[freshProcess].timeBlocked.ns = 0;
				resPTR->pcbTable[freshProcess].timeBlocked.s = 0;

				resPTR->pcbTable[freshProcess].cpuTime.ns = 0;
				resPTR->pcbTable[freshProcess].cpuTime.s = 0;

				maxGenerated--;
				goingProcs++;

				if(resPTR->pcbTable[freshProcess].pClass == 1) {


					enqueue(q2, resPTR->pcbTable[freshProcess].lpid);

					resPTR->pcbTable[freshProcess].precedence = 2;

					if(logLines < maxLogLines) {
						fprintf(filePointer,"OSS: Generating process with PID %d and putting it in queue %d at time %d:%u\n",resPTR->pcbTable[freshProcess].lpid, 2, resPTR->time.s,resPTR->time.ns);
						logLines++;
					}

				}

				if(resPTR->pcbTable[freshProcess].pClass == 2) {


					enqueue(q1, resPTR->pcbTable[freshProcess].lpid);

					resPTR->pcbTable[freshProcess].precedence = 1;

					if(logLines < maxLogLines) {
						fprintf(filePointer,"OSS: Generating process with PID %d and putting it in queue %d at time %d:%u\n",resPTR->pcbTable[freshProcess].lpid,1, resPTR->time.s,resPTR->time.ns);
						logLines++;
					}

				}
			}
		}
		

		/* process starts if a process is not currently running */
		if((qEmpty(q1) == 0 || qEmpty(q2) == 0 || qEmpty(q3) == 0 || qEmpty(q4) == 0) && isGoing == 0) {	
			isGoing = 1;
			/* checks the priority queue */
			int qNum = 0;
			strcpy(msg.msg,"");

			/* queue 0 is removed */
			if (qEmpty(q1) == 0) {
				
				nowActive = dequeue(q1);
				qNum = 1;
				msg.msgType = resPTR->pcbTable[nowActive].pid;
				msgsnd(atKid, &msg, sizeof(msg), 0);

			}

			else if(qEmpty(q2) == 0) {

				nowActive = dequeue(q2);
				qNum = 2;
				msg.msgType = resPTR->pcbTable[nowActive].pid;
				msgsnd(atKid, &msg, sizeof(msg), 0);

			}

			else if(qEmpty(q3) == 0) {

				nowActive = dequeue(q3);
				qNum = 3;
				msg.msgType = resPTR->pcbTable[nowActive].pid;
				msgsnd(atKid, &msg, sizeof(msg),0);

			}

			else if(qEmpty(q4) == 0) {

				nowActive = dequeue(q4);
				qNum = 4;
				msg.msgType = resPTR->pcbTable[nowActive].pid;
				msgsnd(atKid, &msg, sizeof(msg), 0);

			}

			if(logLines < maxLogLines) {
				fprintf(filePointer,"OSS: Dispatching process with PID %d from queue %d at time %d:%d \n", nowActive, qNum, resPTR->time.s, resPTR->time.ns);		
				logLines++;
			}

			/* system clock adjustment */

			int randTimeCost = ((rand()% 100) + 50);
			incrementTime(&(resPTR->time), 0, randTimeCost);	
				
		}
		
		if (qEmpty(blockedProcQ) == 0) {
			int k;
			for(k = 0; k < blockedProcQ->size; k++) {
				int lpid = dequeue(blockedProcQ);
				if((msgrcv(atOSS, &msg, sizeof(msg),resPTR->pcbTable[lpid].pid,IPC_NOWAIT) > -1) && strcmp(msg.msg,"complete") == 0) {
					if(resPTR->pcbTable[lpid].pClass == 1) {
						enqueue(q2, resPTR->pcbTable[lpid].lpid);
						resPTR->pcbTable[lpid].precedence = 2;
					}
					else if(resPTR->pcbTable[lpid].pClass == 2) {
						enqueue(q1, resPTR->pcbTable[lpid].lpid);
						resPTR->pcbTable[lpid].precedence = 1;
					}
					if(logLines < maxLogLines) {
						fprintf(filePointer,"OSS: Unblocking process with PID %d at time %d:%d\n",lpid,resPTR->time.s,resPTR->time.ns);
						logLines++;
					}
				}
				else{
					enqueue(blockedProcQ, lpid);
				}
				
			}
		}

		/* if a child is currently running, it will send a message */
		if (isGoing == 1) {
			isGoing = 0;

			/* blocks until a message is received from child */
			if ((msgrcv(atOSS, &msg, sizeof(msg),resPTR->pcbTable[nowActive].pid, 0)) > -1 ) {
				
				/* message for child termination */
				if (strcmp(msg.msg,"death") == 0) {
					while(waitpid(resPTR->pcbTable[nowActive].pid, NULL, 0) > 0);
					msgrcv(atOSS,&msg, sizeof(msg), resPTR->pcbTable[nowActive].pid,0);
					int time = 0;	
					
					/* total time in the system */
					resPTR->pcbTable[nowActive].systemTime = resPTR->time;
					resPTR->pcbTable[nowActive].cpuTime.ns = resPTR->time.ns - resPTR->pcbTable[nowActive].cpuTime.ns; 
					
					/* percentage of time that was used */
					int timeChunk = atoi(msg.msg);

					if(resPTR->pcbTable[nowActive].precedence == 1) {
						time = q1Quant / timeChunk;
					}

					else if(resPTR->pcbTable[nowActive].precedence == 2) {
						time = q2Quant * (timeChunk / 100);
					}

					else if(resPTR->pcbTable[nowActive].precedence == 3) {
						time = q3Quant * (timeChunk / 100);
					}

					else if(resPTR->pcbTable[nowActive].precedence == 4) {
						time = q4Quant * (timeChunk / 100);
					}

					incrementTime(&(resPTR->time), 0, time);
					incrementTime(&(resPTR->pcbTable[nowActive].cpuTime), 0, time);	

					usedPIDs[nowActive] = 0;	
					pGenCounter++;
					goingProcs--;
			
					if(logLines < maxLogLines) {
						
						fprintf(filePointer,"OSS: Terminating process with PID %d after %d nanoseconds\n",resPTR->pcbTable[nowActive].lpid, resPTR->time.ns);
						logLines++;
					}


					incrementTime(&totalBlocked, resPTR->pcbTable[nowActive].timeBlocked.s, resPTR->pcbTable[nowActive].timeBlocked.ns);
					incrementTime(&cpuTime,resPTR->pcbTable[nowActive].cpuTime.s,resPTR->pcbTable[nowActive].cpuTime.ns);

					int waitSec = resPTR->pcbTable[nowActive].systemTime.s;
					int waitNS = resPTR->pcbTable[nowActive].systemTime.ns;

					waitSec -= resPTR->pcbTable[nowActive].timeBlocked.s;
					waitNS -= resPTR->pcbTable[nowActive].timeBlocked.ns;

					waitSec -= resPTR->pcbTable[nowActive].cpuTime.s;
					waitNS -= resPTR->pcbTable[nowActive].cpuTime.ns;

					incrementTime(&waitTime,waitSec,waitNS);
				}

				/* if the child were to use all of its time */
				if (strcmp(msg.msg,"all time used") == 0) {

					int prevPriority = 0;

					if(resPTR->pcbTable[nowActive].precedence == 1) {

						prevPriority = resPTR->pcbTable[nowActive].precedence;
						resPTR->pcbTable[nowActive].precedence = 2;

						enqueue(q1, resPTR->pcbTable[nowActive].lpid);

						incrementTime(&(resPTR->time) ,0 ,q1Quant);
						incrementTime(&(resPTR->pcbTable[nowActive].cpuTime),0 ,q1Quant);

					}

					else if(resPTR->pcbTable[nowActive].precedence == 2) {

						prevPriority = resPTR->pcbTable[nowActive].precedence;
						resPTR->pcbTable[nowActive].precedence = 3;

						enqueue(q3, resPTR->pcbTable[nowActive].lpid);

						incrementTime(&(resPTR->time), 0,q2Quant);
						incrementTime(&(resPTR->pcbTable[nowActive].cpuTime),0, q2Quant);

					}

					else if(resPTR->pcbTable[nowActive].precedence == 3) {

						prevPriority = resPTR->pcbTable[nowActive].precedence;
						resPTR->pcbTable[nowActive].precedence = 4;

						enqueue(q4, resPTR->pcbTable[nowActive].lpid);

						incrementTime(&(resPTR->time) ,0, q3Quant);
						incrementTime(&(resPTR->pcbTable[nowActive].cpuTime),0 ,q3Quant);

					}

					else if(resPTR->pcbTable[nowActive].precedence == 4) {

						prevPriority = resPTR->pcbTable[nowActive].precedence;
						resPTR->pcbTable[nowActive].precedence = 4;

						enqueue(q4, resPTR->pcbTable[nowActive].lpid);

						incrementTime(&(resPTR->time) ,0 ,q4Quant);
						incrementTime(&(resPTR->pcbTable[nowActive].cpuTime),0, q4Quant);

					}

					if(logLines < maxLogLines) {
						fprintf(filePointer,"OSS: Moving process with pid %d from queue %d to queue %d\n", resPTR->pcbTable[nowActive].lpid, prevPriority, resPTR->pcbTable[nowActive].precedence);
						logLines++;
					}
				}

				/* if the time is used partially */
				if (strcmp(msg.msg,"some time used") == 0) {
					int time = 0;	
					msgrcv(atOSS, &msg, sizeof(msg),resPTR->pcbTable[nowActive].pid,0);
					
					/* percentage of time used */
					int timeChunk = atoi(msg.msg);

					if(resPTR->pcbTable[nowActive].precedence == 1) {
						time = q1Quant  / timeChunk;
					}

					else if(resPTR->pcbTable[nowActive].precedence == 2) {
						time = q2Quant  / timeChunk;
					}

					else if(resPTR->pcbTable[nowActive].precedence == 3) {
						time = q3Quant / timeChunk;
					}	

					else if(resPTR->pcbTable[nowActive].precedence == 4) {
						time = q4Quant  / timeChunk;
					}

					incrementTime(&(resPTR->time),0,time);
					incrementTime(&(resPTR->pcbTable[nowActive].cpuTime),0,time);	

					if (logLines < maxLogLines) {
						fprintf(filePointer,"OSS: Process with PID %d has been preempted after %d nanoseconds\n",resPTR->pcbTable[nowActive].lpid, time);
						logLines++;
					}
					enqueue(blockedProcQ,resPTR->pcbTable[nowActive].lpid);
				}

			}
		}
	}
	pid_t wpid;
	while ((wpid = wait(&pStatus)) > 0);	
	printf("The build process has been completed.\n");
}


int main(int argc, char *argv[]) {
	signal(SIGALRM, killOSS);
	signal(SIGINT, killOSS);
	alarm(3);

	key_t mKey;
	key_t key;
	int opt = 0;

	while ((opt=getopt(argc, argv, "n:s:h"))!= EOF) {
		switch(opt) {
			case 'h':
				printf("\nUse the command in this order: ./oss -t\n");
				printf("-t: Total processes (Maximum of 20) \n\n");
				exit(0);
				break;
			case 't':
				setProcesses = atoi(optarg);
				if (setProcesses > 20) {
					setProcesses = 20;
				}
				break;
		}
	}
	
	/* set the default number of processes to 18 */
	if(setProcesses == 0) {
		setProcesses = 18;
	}

	/* output_log.txt is the file for the logs */
	filePointer = fopen("log.txt", "w");
	printf("Starting scheduler..\n\n");
	printf("Creating log.txt file..\n\n");
	printf("Writing to log.txt..\n");


	/* attaches process control block and system clock to shared memory */
	
	key = ftok(".",'a');
	if ((SHMid = shmget(key,sizeof(struct sharedRes), IPC_CREAT | 0666)) == -1) {
		perror("shmget");
		exit(1);	
	}
	
	resPTR = (struct sharedRes*)shmat(SHMid,(void*)0,0);
	if(resPTR == (void*)-1) {
		perror("Error on attaching memory");
	}

	/* queues for communication between processes */
	
	if((mKey = ftok("qFileOne", 925)) == -1) {
		perror("ftok");
	}

	if((atKid = msgget(mKey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
	}	
	
	if((mKey = ftok("qFileTwo", 825)) == -1) {
		perror("ftok");
	}

	if((atOSS = msgget(mKey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
	}
	
	for(int i = 0; i < 20; i++) {
		usedPIDs[i] = 0;
	}	

	ossScheduler();
	cleanProgram();

	printf("Program finished successfully. Bye.");
	return 0;
}

