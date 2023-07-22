#include "myFunctions.hpp"
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>


#define FIFO_BSIZE 512
#define FILE_READ_SIZE 4096
#define PERMS 0666



using namespace std;


vector<Pair> urls_found;
struct sigaction sigterm;

int fd;
int url_fd;
int out_fd;


//SIGTERM handler checks if all file descriptors are closed and frees leftover heap memory. Then exits.
void sig_term_handler(int sig) {

	if (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
		close(fd);
	
	if (fcntl(url_fd, F_GETFD) != -1 || errno != EBADF)
		close(url_fd);

	if (fcntl(out_fd, F_GETFD) != -1 || errno != EBADF)
		close(out_fd);

	for (int i; i < urls_found.size(); i++){
		if (urls_found[i].url != NULL){
			delete urls_found[i].url;
		}
	}
	
	exit(EXIT_SUCCESS);
 
}



int main(int argc, char* argv[]){

	//Worker acceptes sigterm signal but ignores sigint signal that sniffer sends 
	//so worker can handle any leftover tasks and then exit
	signal(SIGINT, SIG_IGN);

	sigterm.sa_handler = sig_term_handler;
	sigaction(SIGTERM, &sigterm, NULL);

	//Opens named pipe (that was passed as an argument)
	char* fifo = argv[1];
	fd = open(fifo, O_RDONLY);

	if (fd == -1) {
		perror("Worker fifo Open Error");
	}

	char fifo_buffer[FIFO_BSIZE];
	

	while(1){ //Worker completes the assigned task and then raises signal SIGSTOP 

		memset(fifo_buffer, 0, FIFO_BSIZE);

		//Reads from named pipe and stores it into fifo_buffer array 
		while (((read(fd, fifo_buffer, FIFO_BSIZE) == -1) && errno == EINTR));

		//Opens file with the urls that was passed through the named pipe
		if ((url_fd = open(fifo_buffer, O_RDONLY)) < 0) {
			perror("Error Opening URL File");
		}

		//Any result is stored in results/ directory
		char res_dir[] = "../results/";	

		//Creates .out file in results/ directory and opens it 
		char* out_file = create_file_name(fifo_buffer, res_dir);
		out_fd = open(out_file, O_CREAT | O_RDONLY | O_WRONLY, PERMS);
		delete[] out_file;

		if (out_fd == -1) {
			perror("Worker File Open Error");
		}

		int bytes;
		char characters[FILE_READ_SIZE];
		string line = "";

		while (1) {

			//initialize buffer for every loop
			memset(characters, 0, FILE_READ_SIZE);

			//read from urls file and store data in characters array buffer 
			bytes = read(url_fd, characters, FILE_READ_SIZE);

			//Parsing one by one all the words (if \n or space detected then a word has been found) of characters buffer
			//There is no chance of a word inside buffer being cut because 
			//if no \n or space is detected in last word of buffer 
			//then buffer gets new data from read until \n or space found
			for (int i = 0; i < FILE_READ_SIZE; i++) {

				if (characters[i] != '\n' && characters[i] != ' ') {
					line += characters[i];
				} else {
					char word_buffer[line.length() + 1];
					memset(word_buffer, 0, line.length() + 1);
					strcpy(word_buffer, line.c_str());
					parse(word_buffer, urls_found);
					line = "";
				}

			}

			//If there is nothig else to read from file exit from while loop 
			if (bytes == 0)
				break;

		}

		//Writing all urls that were found in url file into .out file
		for (int i = 0; i < urls_found.size(); i++){
			write_file(out_fd, urls_found, i);
		}

		//clear vector from all pairs(url, times)
		urls_found.clear();

		close(out_fd);
		close(url_fd);

		//Stopping Worker
		raise(SIGSTOP);

	}

	close(fd);
    return 0;
}