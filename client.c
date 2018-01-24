#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "inet.h"

// Make the alias global so thread can access.
static char alias[MAX];

/*
Thread handler for getting everything from the user
sent up to the server.
*/
void *write_handler(void *sockfd){
	char out_buffer[MAX];
	int *sock_fd = (int *)sockfd;

	// Zero out the out buffer.
	memset(out_buffer, 0, MAX);

	// Send the alias up to the server.
	sprintf(out_buffer, "//ALIAS: %s", alias);
	write(sock_fd, out_buffer, strlen(out_buffer));

	/*
	Forever get input from the user and send it up.
	NEEDS to do more buffer checking for how much information 
	the user just gave us. 
	*/
	while(1){
		fgets(out_buffer, MAX, stdin);
		strip_newline(out_buffer);
		write(sock_fd, out_buffer, MAX);
		memset(out_buffer, 0, MAX);
	}
}

/*
Main function. Sets up the connection the server and then
spins a thread off to do all the writing to the server.

Then reads from the server stream and prints anything the 
server sends down to this client. 
*/
int main(int argc, char **argv)
{
	int					sockfd, directfd;
	struct sockaddr_in	cli_addr, serv_addr, direct_addr;
	char				s[MAX], in_buffer[MAX*10];
	int					nread;
	pthread_t			write_thread;
	char* 				query;
	char 				server_port[MAX];
	char				server_address[MAX];
	int 				readin;
	char				response[5];

	// Get the active directories: 
	memset((char *) &direct_addr, 0, sizeof(direct_addr));
	direct_addr.sin_family = AF_INET;
	direct_addr.sin_addr.s_addr = INADDR_ANY;
	direct_addr.sin_port = htons(DIRECTORY_CLIENT_PORT);


	if((directfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
		my_error("client: can't open directory stream socket");
	}
	
	if(connect(directfd, (struct sockaddr *) &direct_addr, sizeof(direct_addr)) < 0){
		my_error("client: can't connect to directory server");
	}

	// Print the address and get the alias.
	printf("%s \n", inet_ntoa(direct_addr.sin_addr));
	printf("Enter your username: \n");
	scanf("%s", alias);

	// Read in the list of active chat servers from the 
	// directory server and print to client console.
	readin = read(directfd, in_buffer, MAX*10);
	printf("%s\n", in_buffer);

	// Choose a chat to join and read in response.
	printf("Choose a chat to join: \n");
	scanf("%s", response);

	// Write the response to the directory server.
	write(directfd, response, sizeof(response));

	memset(in_buffer, 0, MAX*10);

	// Read in the server information (address,port). 
	readin =read(directfd, in_buffer, MAX*10-1);
	in_buffer[readin] = '\0';

	// Parse the address and port. 
	char *address, *port;
	address = strtok(in_buffer,",");
	port = strtok(NULL, ",");
	sprintf(server_address, "%s", address);
	sprintf(server_port, "%s", port);

	/*
	Sets the server address sruct info.
	*/
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(server_address);
	serv_addr.sin_port = htons(atoi(server_port));

	

	// Setup the socket fd to send to the server.
	if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
		my_error("client: can't open stream socket");
	}
	
	// Connect to the server and get the socket fd to use for the connection.
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		my_error("client: can't connect to server");
	}

	/*
	Spin off a thread to write everything this client puts into 
	the chat up to the server. 
	*/
	pthread_create(&write_thread, NULL, &write_handler, (void*)sockfd);

	/*
	Read in everything from the socket fd and print it to the 
	stdin of this client.
	*/
	while(1) {
		memset(s, 0, MAX);
		nread = read(sockfd, s, MAX);
		if(nread < 1){
			my_error("nothing to read!");
			break;
		}
		else{
			strip_newline(s);
			printf("%s\n", s);
		}
	}
	close(sockfd);
	return(0);
}
