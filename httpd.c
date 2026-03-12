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

void handle_cgi(FILE *network, char *method, char *path) {

	char *query = strchr(path, '?');
	char *args[64];
	int arg_count = 0;

	char *prog_path = path + 1;
	if (query) {
		*query = '\0';
		args[arg_count++] = prog_path;
		char *token = strtok(query + 1, "&");
		
		while (token && arg_count < 63){
			args[arg_count++] = token;
			token = strtok(NULL, "&");
		
		}
	} else {
		args[arg_count++] = prog_path;
			
	}
	args[arg_count] = NULL;
	

	char tmp_filename[32];
	sprintf(tmp_filename, "cgi_%d.tmp", getpid());
	
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(network, "HTTP/1.0 500 Internal Error\r\n\r\n");
		fflush(network);
		return;
	}

	
	if (pid == 0){
		int fd = open(tmp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		dup2(fd, STDOUT_FILENO);
		close(fd);
		execv(args[0], args);
		printf("Status: 500\nError: Could not execute %s\n", args[0]);
				
		perror("execv failed");
		exit(1);
	}

	waitpid(pid, NULL, 0);
	struct stat st;
	
	if(stat(tmp_filename, &st) == 0){
		fprintf(network, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", (long)st.st_size);
		fflush(network);	
		if (strcmp(method, "GET") == 0){
			FILE *f = fopen(tmp_filename, "r");
			char buf[1024];
			size_t n;
		
			if (f){	
				while ((n = fread(buf, 1, sizeof(buf), f)) > 0){
					fwrite(buf, 1, n, network);
				}
				fclose(f);
			}
		
		}
	}else {
		fprintf(network, "HTTP/1.0 500 Internal Error\r\n\r\n");
		fflush(network);
	}
	unlink(tmp_filename);
	


}





void handle_file(FILE *network, char *method, char *filename){
	struct stat st;
	if(stat(filename, &st) == -1){
		fprintf(network, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n404 not Found");
		fflush(network);
		return;
	}

	fprintf(network, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", (long)st.st_size);
	fflush(network);
	if (strcmp(method, "GET") == 0) {
		FILE *f = fopen(filename, "r");
		if(f) {
			char buf[1024];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
				fwrite(buf, 1, n, network);
			}
			fflush(network);
			fclose(f);
		}
	
	} 
}


void handle_request(int nfd){
	printf("DEBUG: Child %d reached handle_request\n", getpid());	

	FILE *in = fdopen(nfd, "r");
	FILE *out = fdopen(dup(nfd), "w");	



	char *line= NULL;
	size_t size = 0;

	if (!in || !out){
		if (in) fclose(in);
		if (out) fclose(out);
		close(nfd);
		return;
	}

	setvbuf(out, NULL, _IONBF, 0);

	if (getline(&line, &size, in) > 0){
		printf("DEBUG: Received Request: %s", line);
		
		char* header = NULL;
		size_t h_size = 0;
		
		while (getline(&header, &h_size, in) > 0){
			if (strcmp(header, "\r\n\r\n") == 0 || strcmp(header, "\n") == 0) {
				break;
			}
		}	
		free(header);

		char method[16];
		char path[256];
		char protocol[16];

		if (sscanf(line, "%15s %255s %15s", method, path, protocol) == 3) {
			printf("DEBUG: sscanf matches");
			if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0){
				fprintf(out, "HTTP/1.0 501 Not Implemented\r\n\r\n501 Method Not Supported");
				fflush(out);
			}else if(strstr(path, "..")){
				fprintf(out, "HTTP/1.0 403 Permission Denied\r\n\r\n403 Forbidden");
				fflush(out);
			} else if (strncmp(path, "/cgi-like/", 10) == 0){
				printf("DEBUG: Routing to CGI: %s\n", path);

				handle_cgi(out, method, path);
			} else {
				printf("DEBUG: Routing to File: %s\n", path + 1);
				handle_file(out, method, path + 1);
			}
		} else {
			fprintf(out, "HTTP/1.0 400 Bad Request\r\n\r\n400 Bad Request");
			fflush(out);
			
		}
	}	
	fflush(out);
	free(line);
	fclose(in);
	fclose(out);
	printf("DEBUG: Child %d finished request.\n", getpid());


}



void run_service(int fd) {
	struct sigaction sa = {
		.sa_handler = sigchild_handler, .sa_flags = SA_RESTART
	};

	sigaction(SIGCHLD, &sa, NULL);
	
	while(1) {
		int nfd = accept_connection(fd);
				

		printf("established connection");
		if (nfd == -1){
			perror("accept_connection error");
			 continue;

		}
		pid_t pid = fork();
		if (pid < 0){
			perror("fork failed");
		} else if (pid == 0 ){
			close(fd);
			handle_request(nfd);
			printf("child %d  exiting now", getpid());
			exit(0);
		}
		
		close(nfd);
		
	

	}


}


int main(int argc, char* argv[]){
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);


	if(argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	
	int port = atoi(argv[1]);
	int fd = create_service(port);
	if (fd == -1){
		perror("create_service");
		exit(1);
	}

	printf("httpd running on port %d...\n", port);
	run_service(fd);
	return 0;
}
