#include <cstdlib>
#include <dirent.h>
#include <stdio.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#define PERMS 0777

//My functions to make main code cleaner and easier to understand

//This struct contains a specific url and how many times has been found
struct Pair {
	char* url;
	int times;
};



// creates Named_Pipes/ dir if it doesn't exist
void create_fifo_dir(){
	mkdir("../Named_Pipes", PERMS);
}

// creates results/ dir if it doesn't exist
void create_results_dir(){
	mkdir("../results", PERMS);
}

// deletes all .out files from results/ dir before creating new result files
void delete_results(){

	char dirname[] = "../results/";
	DIR *dir_ptr;

	struct dirent *direntp;

	if ((dir_ptr = opendir(dirname)) == NULL) {
		perror("Cannot open ../results/ directory");
	} 
	else {
		while ((direntp = readdir(dir_ptr)) != NULL) {
			if (strstr(direntp->d_name, ".out") != NULL) {
			char file_name[512];
			strcpy(file_name, dirname);
			strcat(file_name, direntp->d_name);
			unlink(file_name);
			}
		}

	closedir(dir_ptr);
	}

}

//Finds and returns the direcory and file name together 
//It is used by sniffer to send the right dir/filename to worker through the named pipe 
char* file_path_name(char* inbuf) {

	const char split[2] = " ";
	char *word[3];

	for (int i = 0; i < 3; i++) {
		if (i == 0) {
			word[i] = strtok(inbuf, split);
		} 
		else {
			word[i] = strtok(NULL, split);
		}
	}

	char *file_path = new char[(strlen(word[0]) + 1 + strlen(word[2]) + 1)];

	strcpy(file_path, word[0]);
	strcat(file_path, word[2]);

	return file_path;
}

//Creates a new .out file in results/ directory based on original file name
char* create_file_name(char* buffer, char* dir){

	int counter = 0;
	for (int i = 0; buffer[i] != 0; i++) {
		if (buffer[i] == '/')
			counter++;
	}

	const char split[2] = "/";
	char *word[counter + 1];

	for (int i = 0; i < (counter + 1); i++) {
		if (i == 0) {
			word[i] = strtok(buffer, split);
		} else {
			word[i] = strtok(NULL, split);
		}
	}

	char* file_name = new char[(strlen(dir)) + 1 + strlen(word[counter]) + 1 + strlen(".out") + 1];
	strcpy(file_name, dir);
	strcat(file_name, word[counter]);
	strcat(file_name, ".out");

	return file_name;
}

//Removes :80 port if exists from a url
char* remove_port(char* url){

	char port[] = ":80";

	int port_length = strlen(port);

	char* new_url;
	if ((new_url = strstr(url, port)) != NULL) {
		memmove(new_url, new_url + port_length, strlen(new_url + port_length) + 1);
	}

	return url;

}

//Checking if the given word is a url
char* url_type(char* word){

	char http[] = "http://";
	char* removed;

	if ((removed = strstr(word, http)) == word) {
		removed += strlen(http);
		if (strncmp(removed, "www.", strlen("www.")) == 0){
			removed += strlen("www.");
		}

		removed = remove_port(removed);

		char splitter[2] = "/";
		char* final_url;
		final_url = strtok_r(removed, splitter, &removed);

		return final_url;
	}
	return NULL;

}

//Writing every url and how many times it is found into .out file
void write_file(int fd, std::vector<Pair>& urls_found, int i){

	char number[10];
	sprintf(number, "%d", urls_found[i].times);
	
	char temp[12];
	char splitter[12] = " ";
	strcat(splitter, number);
	strcpy(temp, splitter);

	urls_found[i].url = (char*) realloc(urls_found[i].url, strlen(urls_found[i].url) + strlen(temp) + 4);

	strcat(urls_found[i].url, temp);
	char *to_write = urls_found[i].url;
	strcat(to_write, "\n");

	write(fd, to_write, strlen(to_write));

	if (urls_found[i].url != NULL)
		delete[] urls_found[i].url;

	urls_found[i].url = NULL;

}

//if the vector already has the url then just increase by one the times field of struct Pair
//if the vector does not have this url then it will be added with times field initialized with 1
void insert_in_vector(std::vector<Pair>& vec, char* url){

	int flag = 0;
	for (int i = 0; i < vec.size(); i++) {

		if (strcmp(vec[i].url, url) == 0){
			vec[i].times++;
			flag = 1;
		}

	}

	if (flag == 0){

		Pair pair;
		pair.times = 1;
		pair.url = new char[strlen(url) + 1];
		strcpy(pair.url, url);
		vec.push_back(pair);

	}

}

//Getting fifo name based on pid of worker
char* get_fifo_name(char* fifo, pid_t pid){

	char str[10];
	sprintf(str, "%d", pid);

	char* fifo_name = new char[strlen(fifo) + 1 + strlen(str) + 1];
	strcpy(fifo_name, fifo);
	strcat(fifo_name, str);

	return fifo_name;

}

//Check if the word starts with http or has http in it
//if the word starts with http then call url_type function to check if it is a genuine url
//if the word has http in it then call url_type and pass the char* that points to http to check again if it is a genuine url
void parse(char* word, std::vector<Pair>& urls_found){

	char http[] = "http://";

	char* url;
	if ((url = url_type(word)) != NULL){
		insert_in_vector(urls_found, url);
	}
	else{
		char* checked = strstr(word, http);
		char* url;
		if (checked != NULL){
			if ((url = url_type(checked)) != NULL) {
				insert_in_vector(urls_found, url);
			}
		} 
	}  

}