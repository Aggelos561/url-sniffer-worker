#include <csignal>
#include <vector>

struct Pair {
	char* url;
	int times;
};


void create_fifo_dir();

void create_results_dir();

void delete_results();

char* file_path_name(char* );

char* create_file_name(char* , char* );

int url_type(char* );

void write_file(int , std::vector<Pair>& , int );

void insert_in_vector(std::vector<Pair>&, char*);

char* get_fifo_name(char* , pid_t );

void parse(char *, std::vector<Pair>& );
