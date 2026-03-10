#define _GNU_SOURCE
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

void sigchild_handler(int s){
	int saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0){
		errno = saved_errno;
	}
}

void handle_file(FILE *network, char *method, char *filename){
	struct stat st;
	if(stat(filename, &st) == -1){
		fprintf(network, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n404 not Found");
		return;
	}

	fprintf(network, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", (long)st.st_size;

	if (strcmp(method, "GET") == 0) {
		FILE *f = fopen(filename, "r");
		if(f) {
			char buf[1024];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
				fwrite(buf, 1, n, network);
			}
			fclose(f);
		}
	} 
}
