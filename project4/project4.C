// this is the final project for Operating Systems
// Do serial-> threading-> report
// make a separate function for reading files, makes re-factoring easier
// things to collect
// * total number of bad files
// * total number of directories (use S_ISDIR of st_mode)
// * total regular files (use S_ISREG of st_mode)
// * total number of special files (block, character, fifo - if not bad or regular its special)
// * total number of bytes used by regular files
// * total number of reg files that contain text
// * total number of bytes used by text files (accumulate sizes)

#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#define BUFSIZE 1024
#define MAX_THREADS 15

sem_t numBadFiles_sem, numDir_sem, numReg_sem, numSpec_sem, numRegBytes_sem, numText_sem, numTextBytes_sem, setfname_sem, readfname_sem;

int numBadFiles, numDir, numReg, numSpec, numText, threadCount;
long numTextBytes, numRegBytes;
string fname;
pthread_mutex_t runningMutex = PTHREAD_MUTEX_INITIALIZER;

void initSemsValues() {
	numBadFiles = 0;
	numDir = 0;
	numReg = 0;
	numSpec = 0;
	numRegBytes = 0;
	numText = 0;
	numTextBytes = 0;

	sem_init(&numBadFiles_sem, 0, 1);
	sem_init(&numDir_sem, 0, 1);
	sem_init(&numReg_sem, 0, 1);
	sem_init(&numSpec_sem, 0, 1);
	sem_init(&numRegBytes_sem, 0, 1);
	sem_init(&numText_sem, 0, 1);
	sem_init(&numTextBytes_sem, 0, 1);
	sem_init(&setfname_sem, 0, 1);
	sem_init(&readfname_sem, 0, 0);
}

void setFname(string *file) {
	sem_wait(&setfname_sem);
	fname = *file;
	sem_post(&readfname_sem);
}

void readFname(char *file) {
	sem_wait(&readfname_sem);
	strcpy(file, fname.c_str());
	sem_post(&setfname_sem);
}

void incrNumBad() {
	sem_wait(&numBadFiles_sem);
	numBadFiles++;
	sem_post(&numBadFiles_sem);
}

void incrNumDir() {
	sem_wait(&numDir_sem);
	numDir++;
	sem_post(&numDir_sem);
}

void incrNumReg() {
	sem_wait(&numReg_sem);
	numReg++;
	sem_post(&numReg_sem);
}

void incrNumSpec() {
	sem_wait(&numSpec_sem);
	numSpec++;
	sem_post(&numSpec_sem);
}

void incrRegBytes(int bytes) {
	sem_wait(&numRegBytes_sem);
	numRegBytes += bytes;
	sem_post(&numRegBytes_sem);
}

void incrNumText() {
	sem_wait(&numText_sem);
	numText++;
	sem_post(&numText_sem);
}

void incrNumTextBytes(int bytes) {
	sem_wait(&numTextBytes_sem);
	numTextBytes += bytes;
	sem_post(&numTextBytes_sem);
}

bool checkIsText(const char *fname, long *totalBytes) {
	int cnt, i, tempfd;
	char c;

	if((tempfd = open(fname, O_RDONLY)) < 0) {
		printf("on file open: %s\n", fname);
		perror("File didn't open");
		//this is where we can track the bad files
		incrNumBad();
		// no need to check anything, this file couldn't be opened, return error
		return false;
	}

	*totalBytes = 0;
	while((cnt = read(tempfd, &c, 1)) > 0 ) {
		cnt++;
		(*totalBytes)++;
		
		if(! (isspace(c) || isprint(c))) {
			close(tempfd);
			return false;			
		}
	}
	close(tempfd);
	return true;
}

void waitForOpenThread(int numThreads) {
	bool first = true;
	while(threadCount >= numThreads) {
		// spin
		if(first) {
			// printf("Waiting for open thread...\n");
			first = false;
		}
	}
}

void tryJoins(vector<pthread_t> &threads) {
	for (vector<pthread_t>::iterator i = threads.begin(); i != threads.end(); ++i) {	
		if(pthread_tryjoin_np(*i, NULL) == EBUSY) {
			continue;
		} else {
			threads.erase(i);
			i--;
		}
	}
}

void incrThreadCount() {
	pthread_mutex_lock(&runningMutex);
		threadCount++;
		pthread_mutex_unlock(&runningMutex);
}

void decrThreadCount() {
	pthread_mutex_lock(&runningMutex);
	threadCount--;
	pthread_mutex_unlock(&runningMutex);
}

void *readFromIn(void *f) {
	char fileName[250];
	long totalBytes = 0;
	bool isText = true;
	//collect information about the file
	struct stat fileInfo;

	readFname(fileName);

	if(stat(fileName, &fileInfo)){
		printf("Stat failed for %s\n", fileName);

	} else if(S_ISREG(fileInfo.st_mode)){
		// up all the regular info numbers
		incrNumReg();
		incrRegBytes(fileInfo.st_size);

		isText = checkIsText(fileName, &totalBytes);
		if(isText) {
			// up all the text file info numbers
			incrNumText();
			incrNumTextBytes(totalBytes);
		} else {
			printf("%s is not a text file\n", fileName);
		}

	} else if (S_ISDIR(fileInfo.st_mode)) {
		incrNumDir();
	} else { //its special
		incrNumSpec();
	}	

	decrThreadCount();
	return 0;
}

void runThreads(int numThreads) {
	string file;
	vector<pthread_t> threads;

	while(getline(cin, file) && !cin.eof()) {
		
		if(strcmp(file.c_str(), "exit") == 0) 
    		break;

    	waitForOpenThread(numThreads);

    	tryJoins(threads);
		setFname(&file);

    	pthread_t thrd;
       	if(pthread_create(&thrd, NULL, readFromIn, 0) == 0) {
       		threads.push_back(thrd);
       		incrThreadCount();
       	} else {
       		printf("Couldn't create a thread for file %s\n", file.c_str());
       	}    	
    }

    for (vector<pthread_t>::iterator i = threads.begin(); i != threads.end(); ++i) {
    	pthread_join(*i, NULL);
    }
}

int main(int argc, char *argv[]) {
    int fdIn, cnt, i, numThreads = MAX_THREADS;
    bool doThreads = false;
    threadCount = 0;
    string file;
    char *outFile;

    struct rusage end;
    struct timeval startTime, endTime, wallStart, wallEnd;

    if(argc == 1) {
    	printf("No threads specified, running serial version\n");
    	outFile = "timeSheet.csv";

    } else if(argc == 2) {
    	if(strcmp(argv[1], "thread") == 0)
    		doThreads = true;
    	outFile = "timeSheet.csv";

    } else if(argc == 3) {
    	if(strcmp(argv[1], "thread") == 0){
    		doThreads = true;
    		numThreads = atoi(argv[2]);
    		numThreads = numThreads > MAX_THREADS ? MAX_THREADS : numThreads;
    		printf("Running threaded version with %d threads\n", numThreads);
    	}    	
    	outFile = "timeSheet.csv";

    } else if(argc == 4) {
    	if(strcmp(argv[1], "thread") == 0){
    		doThreads = true;
    		numThreads = atoi(argv[2]);
    		numThreads = numThreads > MAX_THREADS ? MAX_THREADS : numThreads;
    		printf("Running threaded version with %d threads\n", numThreads);
    	} 

    	outFile = argv[3];

    } else {
    	printf("Too many arguments\n");
    }

    printf("Printing timing data to %s\n", outFile);
    numThreads = numThreads == 0 ? 1 : numThreads;

    gettimeofday(&wallStart, NULL);

    initSemsValues();

    if(doThreads) {
    	
    	runThreads(numThreads);


    } else {
	    while(getline(cin, file)) {
	    	if(strcmp(file.c_str(), "exit") == 0) 
	    		break;
	    	
	    	setFname(&file);
	    	readFromIn(0);
	    	
	    }
	}

	//and thats all for the meat and potatoes. Now we print everything all pretty-like

	gettimeofday(&wallEnd, NULL);
	int res = getrusage(RUSAGE_SELF, &end);

	printf("Bad Files: %d\nDirectories: %d\nRegular Files: %d\nSpecial Files: %d\nText Files %d\n", numBadFiles, numDir, numReg, numSpec, numText);
    printf("Regular File Bytes: %ld\nText File Bytes: %ld\n", numRegBytes, numTextBytes);
	

	endTime = end.ru_utime;
	
	long userTime = (endTime.tv_sec * 1000000 + endTime.tv_usec) /1000;


	long wallTime = ((wallEnd.tv_sec - wallStart.tv_sec) * 1000000 + (wallEnd.tv_usec - wallStart.tv_usec))/1000;


    

    cout << "\nUser Seconds: " << endTime.tv_sec << "." << endTime.tv_usec << endl;

    endTime = end.ru_stime;	
	long sysTime = (endTime.tv_sec * 1000000 + endTime.tv_usec) /1000;

	cout << "System Seconds: " << endTime.tv_sec << "." << endTime.tv_usec << endl;
	cout << "Wall Clock Seconds: " << (wallEnd.tv_sec - wallStart.tv_sec) << "." << (wallEnd.tv_usec - wallStart.tv_usec) << endl;
	
    std::ofstream timeSheet;
    string firstline = "";
    std::ifstream infile(outFile);
    if(!infile.good())
    	firstline = "NumberOfThreads,Wall Time,User Time,System Time,Number Bad Files,Number Directories,Number Regular Files,Number Special Files,Number Text Files,Regular Bytes,Text Bytes\n";
    infile.close();

    timeSheet.open(outFile, std::ios_base::app);
    numThreads = doThreads ? numThreads : 0; // if we didn't use threads, set numThreads to 0

    // if this is the first time we make a timeSheet,
    // it will add the headers row, otherwise firstline is empty
    timeSheet << firstline; 
    timeSheet << numThreads << "," << wallTime << "," << userTime << "," << sysTime << "," << numBadFiles << "," << numDir << "," << numReg << "," << numSpec << "," << numText << "," << numRegBytes << "," << numTextBytes << endl;
    timeSheet.close();
}
