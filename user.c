
#include "config.h"

int SHMid;
int atKid = 0; 
int atOSS = 0;
const int chanceToDie = 25;
const int chanceToTakeFullTime = 75;
struct times timeBlocked;
struct sharedRes *resPTR;
key_t key;
key_t mKey;
time_t t;
char temp[25];

void incrementTime(struct times* time, int sec, int ns) {
	time->ns += ns;
	time->s += sec;

	while(time->ns >= 1000000000){
		time->s++;
		time->ns -=1000000000;
	}
}

int main(int argc, char *argv[]) {

	int id = getpid();
	int pid = atoi(argv[0]);

	time(&t);	
	srand((int)time(&t) % id);
	
	/* attaches process control block and system clock to shared memory */

	key = ftok(".",'a');
	if ((SHMid = shmget(key,sizeof(struct sharedRes),0666)) == -1) {
		perror("shmget");
		exit(1);	
	}
	
	resPTR = (struct sharedRes*)shmat(SHMid,(void*)0,0);
	if (resPTR == (void*)-1) {
		perror("Error on attaching memory");
		exit(1);
	}

	/* queues for communication between processes */

	if((mKey = ftok("qFileOne", 925)) == -1) {
		perror("ftok");
		exit(1);
	}

	if((atKid = msgget(mKey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");
		exit(1);
	}	
	
	if((mKey = ftok("qFileTwo",825)) == -1) {
		perror("ftok");
		exit(1);
	}

	if((atOSS = msgget(mKey, 0600 | IPC_CREAT)) == -1) {
		perror("msgget");	
		exit(1);
	}

	while(1) {
		/* chance of termination */
		int chanceOfDeath = (rand()%100);
		msgrcv(atKid,&msg,sizeof(msg), id, 0);

		if (chanceOfDeath <= chanceToDie) {
			msg.msgType = id;
			strcpy(msg.msg,"death");
			msgsnd(atOSS, &msg, sizeof(msg), 0);
			char temp[25];
			sprintf(temp,"%d",chanceOfDeath);
			strcpy(msg.msg,temp);
			msgsnd(atOSS,&msg, sizeof(msg),0);
			return 0;
		}

		int chanceMaxTime = (rand()%100);

		if (chanceMaxTime <= chanceToTakeFullTime) {
			msg.msgType = id;
			strcpy(msg.msg,"all time used");
			msgsnd(atOSS, &msg, sizeof(msg), 0);

		} else {

			timeBlocked.s = resPTR->time.s;
			timeBlocked.ns = resPTR->time.ns;
			int seconds = (rand() % 5) + 1;
			int nanoseconds = ((rand() % 1000) + 1);

			incrementTime(&(timeBlocked), seconds, nanoseconds);
			incrementTime(&(resPTR->pcbTable[pid].timeBlocked),seconds, nanoseconds); 
			msg.msgType = id;
			strcpy(msg.msg,"some time used");
			msgsnd(atOSS, &msg, sizeof(msg), 0);

			sprintf(temp,"%d", (rand()%100) + 1);
			strcpy(msg.msg, temp);			
			msgsnd(atOSS, &msg, sizeof(msg), 0);
			while(1) {
				if(((resPTR->time.s > timeBlocked.s) || (resPTR->time.s == timeBlocked.s && resPTR->time.ns >= timeBlocked.ns))){
					break;
				}
			}
			msg.msgType = id;
			strcpy(msg.msg,"complete");
			msgsnd(atOSS, &msg, sizeof(msg), IPC_NOWAIT);
		}
	}
	shmdt((void*)resPTR);
	return 0;
}
