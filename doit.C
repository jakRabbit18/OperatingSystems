// Everett Harding
// Operating Systems Project 1

#include <sys/time.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <vector>

using namespace std;
typedef struct job {int pid; string command; bool completed; struct timeval start;} JOB;

// this template was kindly posted by Evan Teran
// in this stack overflow post 
//  https://stackoverflow.com/questions/236129/most-elegant-way-to-split-a-string
// =========================================
template<typename Out>
void split(const string &s, char delim, Out result) {
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim)) {
        *(result++) = item;
    }
}


vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, back_inserter(elems));
    return elems;
}
// ==========================================

// ==========================================
void printStatistics(struct rusage *usage, long walltime, int pid, string name) {
	long userSec = usage->ru_utime.tv_sec;
	long sysSec  = usage->ru_stime.tv_sec;
	long userMil = usage->ru_utime.tv_usec;
	long sysMil  = usage->ru_stime.tv_usec;
	long usermillis = (userSec * 1000000 + userMil)/1000;
	long sysmillis = (sysSec * 1000000 + sysMil)/1000;
	cout << "\nProcess " << pid << ", " << name << " Statistics";
	cout << "\nWall-clock Time: " << walltime << " ms";
	cout << "\nMajor Faults: " << usage->ru_majflt;
	cout << "\nMinor Faults: " << usage->ru_minflt;
	cout << "\nVoluntary Pre-empts: " << usage->ru_nvcsw; 
	cout << "\nInvoluntary Pre-empts: " << usage->ru_nivcsw;
	cout << "\nUser CPU Time: " << usermillis << " ms";
	cout << "\nSystem CPU Time: " << sysmillis << " ms\n\n";
}


// ==========================================
int runcommand(int argc, char **argv, bool isBackgroundTask, vector<JOB> *jobs) {	
	
	int pid, i;
	struct timeval t_before, t_after;

	pid = fork();
	gettimeofday(&t_before, NULL);

	if(pid < 0) {
		cerr << "Fork error\n";
		return -1;
	} else if(pid == 0) {
		//this is the child, do kid stuff
		//collect arguments to pass to execvp
		char *newArgs[argc + 1];
		for(i = 0; i < argc; i ++){
			newArgs[i] = argv[i];
		}

		newArgs[argc] = NULL;
		// if(jobs) {
		// 	JOB *job = new JOB;
		// 	job->start = t_before;
		// 	job->pid = pid;
		// 	job->command = string(newArgs[0]);
		// 	job->completed = false;
		// 	jobs->push_back(*job);

		// 	cout << "Numjobs: " << jobs->size() << endl;
		// 	cout << jobs << endl;
		// }	
		if(execvp(newArgs[0], newArgs) < 0){ 
			cout << "Execvp error\n"; 
			return -1;
		}
		
	} else if (pid > 0) {
		//this is the parent, do parent stuff
		gettimeofday(&t_after, NULL);
		cout <<  jobs << endl;
		cout << (isBackgroundTask ? "isBackgroundTask" : "is NOT BackgroundTask") << endl;
		int newPID = isBackgroundTask ? waitpid(-1, 0, WNOHANG) : wait(0);
		int i = 0;
		if (newPID) {
			JOB j;
			if(jobs) for(vector<JOB>::iterator t = jobs->begin(); t < jobs->end(); t++, i++) {
				j = *t;
				cout << j.pid << ", " << j.command << endl;
				if(j.pid == newPID) {
					jobs->erase(jobs->begin() + i);
				}
			}
			long start_sec = j.pid ? j.start.tv_sec : t_before.tv_sec;
			long start_usec = j.pid ? j.start.tv_usec : t_before.tv_usec;

			
			long walltime = ((t_after.tv_sec - start_sec) * 1000000 + t_after.tv_usec - start_usec)/1000; 
			struct rusage usage;
			getrusage(RUSAGE_CHILDREN, &usage);
			printStatistics(&usage, walltime, newPID, j.command);
		}

		return pid;
	}
}

int main (int argc, char **argv){
	int i;
	if(argc > 1) {
		char *newArgs[argc];
		for(i = 0; i < argc-1; i ++){
			newArgs[i] = argv[i+1];
		}
		newArgs[argc-1] = NULL;

		int result = runcommand(argc, newArgs, false, NULL);
	} else if(argc == 1) {

		//enter prompt loop
		bool complete = false;
		char *prompt = new char[1];
		prompt[0] = '>';
		vector<JOB> *jobs = new vector<JOB>;
		while(!complete) {
			//get user input
			string arguments;
			printf("%s", prompt);
			getline(cin, arguments);

			if(!arguments.length()) {
				continue;
			}
			
			//parse arguments
			vector<string> str = split(arguments, ' ');
			vector<string>::iterator it;
			char **cmds = new char*[str.size()];
			int i = 0;
			bool isBackgroundTask = false;
			for(it = str.begin(); it < str.end(); it++, i++){
				cmds[i] = new char[(*it).length() + 1];
				strcpy(cmds[i], (*it).c_str());
			}

			if((*(--it)).compare("&") == 0) {
				isBackgroundTask = true;
				str.pop_back();
			}

			if(str[0].compare("exit") == 0) {// if it is exit, then exit the loop
				return 0;

			} else if (str[0].compare("cd") == 0) {	 // if it is cd, change the directory
				string nextDir = str[1];
				chdir(str[1].c_str());				

			} else if (str.size() == 4 && str[0].compare("set") == 0 && str[1].compare("prompt") == 0)	{
				// if it is set prompt, update the prompt
				delete prompt;
				prompt = new char[str[3].length()+1];
				strcpy(prompt, str[3].c_str());
			} else if (str[0].compare("jobs") == 0) { // if it is jobs, list all the current jobs
				for(vector<JOB>::iterator iter = jobs->begin(); iter < jobs->end(); iter++) {
					JOB job = *iter;
					if(! job.completed) {
						cout << "PID: " << job.pid << "  " << job.command << endl;
					}
				}
			} else {
				// otherwise, fork it and make the kids do it
				int result = runcommand(str.size(), cmds, isBackgroundTask, jobs);
				if(result >=0) {
					printf("adding %s, %s to the jobs list\n", result, cmds[0] );
					JOB *j = new JOB;
					j->pid = result;
					j->command = cmds[0];
					j->completed = false;
					gettimeofday(&(j->start), NULL);
					jobs->push_back(*j);
				}
			}			

		}
	}


	
}


