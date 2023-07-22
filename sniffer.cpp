#include "myFunctions.hpp"
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <queue>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define LISTENER_NAME "inotifywait"
#define LISTENER_READ_BUF 1
#define WRITE_BUFFER 512
#define READ 0
#define WRITE 1
#define WORKER "./worker"
#define PERMS 0666

using namespace std;

queue<pid_t> available_workers;
vector<pid_t> pids;
pid_t listener;

struct sigaction sgchild;
struct sigaction sgint;

// SIGINT hanler kills with SIGKILL the listerner process and then sends SIGTERM
// to all the processes that are stored in a vector 
// (If the process is stopped then SIGTERM will be a pending signal until SIGCONT signal is sent)
void sig_int_handler(int sig) {

	kill(listener, SIGKILL);

	for (int pid = 0; pid < pids.size(); pid++){
		kill(pids[pid], SIGTERM);
		kill(pids[pid], SIGCONT);
	}

	exit(EXIT_SUCCESS);

}


//SIGSTOP handler waits any worker to stop and then inserts the pid into the queue
//to be available again
void sig_stop_handler(int sig){

	int worker;
	int status;

	while ((worker = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		available_workers.push(worker);
	}

}


int main(int argc, char* argv[]) {

	sgint.sa_handler = sig_int_handler;
	sigaction(SIGINT, &sgint, NULL);

	sgchild.sa_handler = sig_stop_handler;
	sgchild.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sgchild, NULL);

	//Max directory length for inotifywait is 4096 bytes
	char inotify_dir[4096];

	//Checking Sniffer arguments
	if (argc == 1){

		char curr_dir[] = ".";
		strcpy(inotify_dir, curr_dir);

	}
	else if (argc == 3) {

		if (strcmp(argv[1], "-p") == 0){
			strcpy(inotify_dir, argv[2]);
		}
		else{
			perror("Command line arguments error");
			exit(EXIT_FAILURE);
		}

	}
	else{

		perror("Command line arguments error");
		exit(EXIT_FAILURE);

	}

	//Creating Named_Pipes/ directory where fifo files will be stored
	create_fifo_dir();
	
	//Creating results/ directory where all .out files will be stored
	create_results_dir();

	//Deleting any previous .out files
	delete_results();

	//Pipe between Sniffer and Listener
	int fd[2];
	if (pipe(fd) == -1) {
		perror("Pipe Error\n");
		exit(EXIT_FAILURE);
	}

	//fork --> execlp listener
	listener = fork();

	if (listener == -1) {
		perror("Error Creating Listener");
	}
	else if (listener == 0) {

		//sigint is ignored because sniffer will only kill this process with sigkill
		//by enabling sigint on listener when sniffer is interrupted with sigint, listener also gets this signal.
		signal(SIGINT, SIG_IGN);

		//Listener should not read from pipe, only write
		//Listener now writes in pipe instead of stdout stream
		close(fd[READ]);
		dup2(fd[WRITE], 1);
		close(fd[WRITE]);

		//execlp with -m -e create -e moved_to so listener only responds(writes) when a file is created or moved in a given direcory
		int exec_res = execlp(LISTENER_NAME, LISTENER_NAME, inotify_dir, "-m", "-e", "create", "-e", "moved_to", NULL);

		if (exec_res == -1){
			perror("Error Executing Listener");
		}

	}

	//Sniffer (manager) should only read from pipe
	close(fd[WRITE]);
	dup2(fd[READ], 0);
	close(fd[READ]);

	char inbuf[WRITE_BUFFER];
	char character;

	while (1) {

		character = ' ';
		memset(inbuf, 0, WRITE_BUFFER);
		
		//Sniffer reads from the pipe one by one the data that the listener writes
		//Its safer to read one by one in this situation because data size is small 
		//but lenth of data is not always the same. (it depends by directory length and file name size)
		//By reading one by one if a user moves lots of files simultaneously workers won't miss any given file
		int bytes = 1;
		while(character != '\n'){
			while (((bytes = read(READ, &character, LISTENER_READ_BUF)) == -1) && errno == EINTR);

			if (character != '\n')
				strncat(inbuf, &character, 1);

			//If there is no data to read from pipe then Listener's directory path to watch does not exit
			//and listener has already finished and terminated
			if (bytes == 0){
				perror("Sniffer-Listener Pipe Error");
				kill(listener, SIGKILL);
				exit(EXIT_FAILURE);
			}

		}


		//Path for Sniffer-Workers named pipes
		char workers_fifo_path[] = "../Named_Pipes/wpipe_";
		
		//If the is no available worker in queue(because the program just started or because all workers are busy) 
		//then a new worker is created(fork --> execl)
		if (available_workers.empty()){

			pid_t worker;
			worker = fork();

			//inserting every worker's pid so all processes can 
			//be terminated immediately when needed 
			pids.push_back(worker);

			if (worker == -1) {
				perror("Error Creating Worker");
			} 
			else if (worker == 0) {
				
				char *fifo = get_fifo_name(workers_fifo_path, getpid());
				
				//Passing named pipe name as argument in execl
				int exec_res = execl(WORKER, WORKER, fifo, NULL);

				delete [] fifo;
				if (exec_res == -1) {
					perror("Error Creating Worker");
				}

			}
			else{
				//Named pipe name is based on worker's pid 
				char *fifo = get_fifo_name(workers_fifo_path, worker);

				//Manager must make the named pipe for the specific worker that was created
				if (mkfifo(fifo, PERMS) < 0 && errno != EEXIST) {
					perror("Can't Create Workers FIFO");
				}

				//Opening named pipe 
				int fifo_desc = open(fifo, O_WRONLY);

				if (fifo_desc == -1) {
					perror("Sniffer fifo Open Error");
				}
				
				//Writing file name into the named pipe by removing MOVED_TO or CREATE and concat the dir name and file name
				char* file_path = file_path_name(inbuf);
				write(fifo_desc, file_path , WRITE_BUFFER);
				
				delete[] fifo;
				delete[] file_path;

				close(fifo_desc);
			
			}

		}
		else{	//If any worker is available (Exists and not busy)

			int worker = available_workers.front();  //pop pid of the available worker
			available_workers.pop();

			//Get name of named pipe (whitch is already been created when this worker was created) and open it
			char *fifo = get_fifo_name(workers_fifo_path, worker);
			int fifo_desc = open(fifo, O_WRONLY);

			if (fifo_desc == -1){
				perror("Sniffer fifo Open Error");
			}
			
			
			//Write in named pipe
			char *file_path = file_path_name(inbuf);
			write(fifo_desc, file_path, WRITE_BUFFER);

			delete[] fifo;
			delete[] file_path;

			//Send SIGCONT to this worker to complete the task
			kill(worker, SIGCONT);

			close(fifo_desc);

		}

	}

  	return 0;
}
