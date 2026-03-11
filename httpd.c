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

	fprintf(network, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", (long)st.st_size);

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


void handle_request(int nfd){
	FILE *network = fdopen(nfd, "r+");
	char *line= NULL;
	size_t size = 0;
	
	if (getline(&line, &size, network) > 0){
		char method[16];
		char path[256];
		char protocol[16];

		if (sscanf(line, "%s %s %s", method, path, protocol) == 3) {
			if(strstr(path, "..")){
				fprintf(network, "HTTP/1.0 403 Permission Denied\r\n\r\n403 Forbidden");
			} else if (strncmp(path, "/cgi-like/", 10) == 0){
				handle_cgi(network, method, path);
			} else {
				handle_file(network, method, path + 1);
			}
		} else {
			fprintf(network, "HTTP/1.0 400 Bad Request\r\n\r\n400 Bad Request");
		}
	}	
	fflush(network);
	free(line);
	fclose(network);



}
