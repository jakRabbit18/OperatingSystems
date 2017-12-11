// multiple adder threads add given values and return the cumulative
// total upon termination
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;
#include <vector>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <sstream>
#include <fstream>
#include <fcntl.h>

#define  MAX_ADDERS 10

void *adder(void *);
void *mainThread(void *);
int sendMessage(int, struct msg);
int NBSendMessage(int, struct msg);
int recieveMessage(int, struct msg*);

typedef 
	struct mailflag{
		int boxID;
		pthread_t thread; //number of the mailbox to which this applies
		sem_t send; //semaphore to signal when time to send message
		sem_t recieve; //semaphore to signal time to recieve message
	} MAILFLAG;

struct msg {
	int iFrom;   // thread that send the message
	long value;  // value of the message
	long cnt;    // number of messages recieved
	long tot;    // total time (in milliseconds)
};

struct unsentMsg {
	struct msg message;
	int toThread;
};

vector<MAILFLAG> mflags;
vector<struct msg> mailboxes;
vector<struct unsentMsg> unsent;

int main(int argc, char **argv) {
	// first, initiate all the semaphores (one per mailbox)
	int numThreads, i;
	char *fileName;
	numThreads = MAX_ADDERS;

	if(argc == 2){
		// only 2 args, so assume 2nd is the maximum number of threads
		numThreads = atoi(argv[1]) > MAX_ADDERS ? MAX_ADDERS : atoi(argv[1]);
	}

	cout << "\nNumber of threads: " << numThreads << '\n' << endl;;

	mflags.reserve(numThreads + 1);
	mailboxes.reserve(numThreads + 1);
	unsent.reserve(1);

	// once they terminate, time to clean up	
	for(i = 0; i <= numThreads; i++){

		//dummy message to fill a mailbox
		struct msg m = {0,0,0,0};
		mailboxes.push_back(m);

		cout << "Box number: " << i;
		MAILFLAG *mflag = new MAILFLAG;

		sem_init(&(mflag->send), 0, 1);
		sem_init(&(mflag->recieve), 0, 0);


		mflag->boxID = i;
		mflags.push_back(*mflag);
		

		int check;
		sem_getvalue(&(mflag->send), &check);
		cout << "\tSend: " << check;
		sem_getvalue(&(mflag->recieve), &check);
		cout << "\tRecieve: " << check << endl;
	}

	 // next, start up the threads
	std::vector<MAILFLAG>::iterator it;
	for(it = mflags.begin(); it < mflags.end(); it++) {
		if(it != mflags.begin()) {
			if (pthread_create(&(it->thread), NULL, adder, (void *)&(it->boxID)) != 0) {
				printf("pthread_create");
				exit(1);
			}
			printf("Making thread #%d \n", it->boxID);
		}
	}

	//this is thread 0, make it the main thread
	if (pthread_create(&(mflags.begin()->thread), NULL, mainThread, (void *)&numThreads) != 0) {
		printf("pthread_create");
		exit(1);
	}
	// the main thread is going to do the heavy lifting, but we need to wait for that to terminate
	// once they terminate, time to clean up
	for(it = mflags.begin(); it < mflags.end(); it++) {
		pthread_join(it->thread, NULL);
		sem_destroy(&(it->send));
		sem_destroy(&(it->recieve));
	}

	printf("Fin\n");
	return 0;
}

int sendMessage(int toThread, struct msg message) {
	
	sem_wait(&(mflags[toThread].send));
	mailboxes[toThread] = message;
	sem_post(&(mflags[toThread].recieve));
	return message.value;
}

int NBSendMessage(int toThread, struct msg message) {
	sem_trywait(&(mflags[toThread].send));
	if(errno == EAGAIN){
		errno = 0;
		return -1;
	} else {
		mailboxes[toThread] = message;
		sem_post(&(mflags[toThread].recieve));
		return message.value;
	}
}

int recieveMessage(int boxNum, struct msg *message) {
	sem_wait(&(mflags[boxNum].recieve));
	int val = mailboxes[boxNum].value;
	*message = mailboxes[boxNum];
	sem_post(&(mflags[boxNum].send));
	return val;
}

//use this as the main thread 
void *mainThread(void *args) {
	int k, count = 0, mVal = 0, toThread = 0, numThreads = *(int*) args;
	while (scanf("%d", &k) == 1) {
		if(count++ % 2) {
			toThread = k;
			struct msg m = {0, mVal, -1,-1};
			if(toThread <= numThreads) {
				int result = NBSendMessage(toThread, m);
				if(result == -1) {
					struct unsentMsg um = {m, toThread};
					unsent.push_back(um);
					cout << "Couldn't send message to " << toThread << endl;
				}
			} 
			else {
				printf("No such thread: %d\n", toThread + 1);
			}
		} else {
			mVal = k;
		}
	}
	cout << "Total unsent messages: " <<  unsent.size() << endl;

	while (!unsent.empty()) {
		//always send the front of the list first
		struct unsentMsg um = unsent.back();

		sendMessage(um.toThread, um.message);

		unsent.pop_back();
	}

	for (std::vector<MAILFLAG>::iterator i = mflags.begin() + 1; i != mflags.end(); ++i) {
		struct msg terminate;
		terminate.iFrom = 0;
		terminate.value = -1;
		sendMessage(i->boxID, terminate);
	}

	int numTerminated = 0; 
	while(numTerminated < numThreads) {
		struct msg m;
		recieveMessage(0, &m);
		printf("Thread %d returned %d after %d operations in %ld milliseconds\n", m.iFrom, m.value, m.cnt, m.tot);
		numTerminated++;
	}

}

void *adder(void *args) {
	int boxNum = *(int *)args;
	long messageCount, total;
	struct msg message;
	total = 0;
	messageCount = 0;
	struct timeval startTime;
	struct timeval endTime;
	gettimeofday(&startTime, NULL);
	int val;
	do{
		// get the message
		val = recieveMessage(boxNum, &message);
		if(val >= 0) {
			total += val;
			messageCount ++;
		}
		
	} while(val >= 0);
	gettimeofday(&endTime, NULL);

	long start_sec = startTime.tv_sec;
	long start_usec = startTime.tv_usec;
	long end_sec = endTime.tv_sec;
	long end_usec = endTime.tv_usec;
	long walltime = ((end_sec - start_sec) * 1000000 + (end_usec - start_usec))/1000; //TODO check how this calculates everything

	struct msg term = {boxNum, total, messageCount, walltime};
	sendMessage(0, term);
}


