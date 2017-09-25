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
int runcommand(int argc, char **argv, JOB* j) {	
	
	int i, pid;
	pid = j->pid;
	struct timeval t_before, t_after;
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
		
		if(execvp(newArgs[0], newArgs) < 0){ 
			cout << "Execvp error\n"; 
			return -1;
		}
		
	} else if (pid > 0) {
		//this is the parent, do parent stuff

		
		int newPID = wait(0);
		gettimeofday(&t_after, NULL);
		if (newPID) {
			
			long start_sec = j ? j->start.tv_sec : t_before.tv_sec;
			long start_usec = j ? j->start.tv_usec : t_before.tv_usec;
			long end_sec = t_after.tv_sec;
			long end_usec = t_after.tv_usec;
			long walltime = ((end_sec - start_sec) * 1000000 + (end_usec - start_usec))/1000; 
			struct rusage usage;
			getrusage(RUSAGE_CHILDREN, &usage);
			printStatistics(&usage, walltime, newPID, "" ); // j.command
		}

		return newPID;
	}
	return -1;
}

int main (int argc, char **argv){
	int i;
	if(argc > 1) {
		char *newArgs[argc];
		for(i = 0; i < argc-1; i ++){
			newArgs[i] = argv[i+1];
		}
		JOB * j = new JOB;
		newArgs[argc-1] = NULL;
		int pid = fork();
		j->pid = pid;
		j->command = newArgs[0];
		j->completed = false;
		gettimeofday(&(j->start), NULL);
		int result = runcommand(argc, newArgs, j);
	} else if(argc == 1) {

		//enter prompt loop
		bool complete = false;
		char *prompt = new char[1];
		prompt[0] = '>';
		vector<JOB*> *jobs = new vector<JOB*>;
		while(!complete) {
			//get user input
			string arguments;
			printf("%s", prompt);
			getline(cin, arguments);

			if(!arguments.length()) {
				continue;
			}
			
			// parse arguments =========
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

			// end parsing ===============

			if(str[0].compare("exit") == 0) {// if it is exit, then exit the loop
				exit(0);

			} else if (str[0].compare("cd") == 0) {	 // if it is cd, change the directory

				string nextDir = str[1];
				chdir(str[1].c_str());				

			} else if (str.size() == 4 && str[0].compare("set") == 0 && str[1].compare("prompt") == 0)	{
				// if it is set prompt, update the prompt
				delete[] prompt;
				prompt = new char[str[3].length()+1];
				strcpy(prompt, str[3].c_str());

			} else if (str[0].compare("jobs") == 0) { // if it is jobs, list all the current jobs
				for(vector<JOB*>::iterator iter = jobs->begin(); iter < jobs->end(); iter++) {
					JOB *job = *iter;
					if(! job->completed) {
						cout << "PID: " << job->pid << "  " << job->command << endl;
					}
				}

			} else {
				// otherwise, fork it and make the kids do it
				// make a new job to be started
				JOB *j = new JOB;
				j->command = cmds[0];
				j->completed = false;
				gettimeofday(&(j->start), NULL);

				jobs->push_back(j);
				j->pid = fork();

				int result = runcommand(str.size(), cmds, j);

				JOB *jo;
				i = 0;
				for(vector<JOB*>::iterator t = jobs->begin(); t < jobs->end(); t++, i++) {
					jo = *t;
					if(jo->pid == result) {
						jobs->erase(jobs->begin() + i);
					}
					if(jo->pid == 0) {
						jobs->erase(jobs->begin() + i);
					}
					if(result < 0 && jo->pid == j->pid) {
						jobs->erase(jobs->begin() + i);
					}
				}
			}			
		}
	}


	
}


