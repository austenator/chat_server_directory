#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "inet.h"

static int num_clients = 0;
static fd_set fds;
static first = 1;
static char* client_names[MAX_CLIENTS];
static char server_name[MAX];
static int 	tcp_port;

/*
Could not get the duplicate name checking to work, 
but the concepts should work! These are helper functions for that.

// Adds a client name to the global name list.
void add_client_name(char* name){
	for(int i = 0; i < MAX_CLIENTS; i++){
		if(client_names[i][0] == '\0'){
			sprintf(client_names[i], name, strlen(name));
			break;
		}
	}
}

// Removes a client name from the global name list.
void remove_client_name(char* name){
	for(int i = 0; i < MAX_CLIENTS; i++){
		if(client_names[i][0] != '\0'){
			if(strcmp(client_names[i], name)){
				client_names[i][0]='\0';
				break;
			}
		}
	}
}

// Returns 1 or 0 based on if the name passed in is a duplicate.
int duplicate_client_name(char* name){
	for(int i = 0; i < MAX_CLIENTS; i++){
		if(client_names[i][0] != '\0'){
			if(strcmp(client_names[i], name)){
				return 1;
			}
		}
	}
	return 0;
}
*/

// Send message to all but the sender.
void send_message(char *message, int sender_fd){
	strip_newline(message);
	for(int i=0;i<MAX_CLIENTS;i++){
		if(FD_ISSET(i, &fds)){
			if(i != sender_fd){
				write(i, message, strlen(message));
			}
		}
	}
}

/*
Handles a new client. Reads client's input and based
on the input does different functions. Gets the alias from
client code, and sends client messages to everyone in the chat room.
Also tells the client if they are the first in the chat room and
alerts the other users that this client joined. 
*/
void *client_handler(void *newsockfd){
	char in_buffer[MAX];
	char out_buffer[MAX];
	int readinlen;
	char alias[MAX];

	int *new_sockfd = (int *)newsockfd;
	memset(in_buffer, 0, MAX);

	/*
	If this is the first client on the server (chat room),
	write to the client that they are the first person.
	Otherwise send t
	*/
	if(first){
		char *welcome_msg = "You are the first person in the chat room!\n";
		write(new_sockfd, welcome_msg, strlen(welcome_msg));
		first = 0;
	}

	/* 
	This if statement is supposed to check if the client's
	name is a dpulicate. If it is, write an error message to the client
	and then close the server (there could be better ways, but for this 
	assignment that isn't necessary).
	*/

	/*if(duplicate_client_name(client_names)){
		char* duplicate_error = "Can't have duplicate names. Exiting.";
		my_error(duplicate_error);
		write(new_sockfd, duplicate_error, strlen(duplicate_error));
	}
	else{*/
		while((readinlen = read(new_sockfd, in_buffer, sizeof(in_buffer)-1)) > 0){
			in_buffer[readinlen] = '\0';
			out_buffer[0]='\0';
	
			// If nothing in the in buffer, go ahead and keep reading.
			if(!strlen(in_buffer)){
				continue;
			}
	
			// This means it is a code message from client.
			if(in_buffer[0] == '/'){
				char *command, *param;
				command = strtok(in_buffer," ");

				// If the command is an alias, parse it.
				if(!strcmp(command, "//ALIAS:")){
					
					// Get the alias and store it.
					param = strtok(NULL, " ");
					sprintf(alias, "%s", param);

					/* 
					If this isn't the first person in the chat room,
					send a message to the other clients that this 
					person has joined the chat.
					*/ 
					if(!first){
						sprintf(out_buffer, "%s has joined the chat! \n", alias);
						send_message(out_buffer, new_sockfd);
					}
				}
	
			}
			else{
				/*
				If this isn't a command, just read it into the out 
				buffer and send the message to the other clients in 
				the chat room. 
				*/
				sprintf(out_buffer, "%s: %s", alias, in_buffer);
				send_message(out_buffer, new_sockfd);
			}
		}
	//}

	/* 
	Do client exiting functionality. Close the client socket fd,
	remove it from the active fd_set, close the thread, and return null.
	We are done with this server-client interaction. 
	*/
	printf("Exiting thread. \n");
	close(new_sockfd);
	FD_CLR(*new_sockfd, &fds);
	pthread_detach(pthread_self());
	return NULL;
}

/*
Handles sending the name, address, and port of this chat server
to the directory server to register it with the master active chat
server list.
*/
void* directory_handler(void){
	int sockfd, nread;
	struct sockaddr_in 	dir_addr;
	char buffer[MAX];

	memset((char *) &dir_addr, 0, sizeof(dir_addr));
	dir_addr.sin_family = AF_INET;
	dir_addr.sin_addr.s_addr = INADDR_ANY;
	dir_addr.sin_port = htons(DIRECTORY_SERVER_PORT);
	

	if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
		my_error("server: can't open stream socket in directory handler");
		}
	
	// Connect to the server and get the socket fd to use for the connection.
	if(connect(sockfd, (struct sockaddr *) &dir_addr, sizeof(dir_addr)) < 0){
		my_error("server: can't connect to server in directory handler");
		}

	// Format the necessary information and write it to the directory server.
	sprintf(buffer, "%s,%s,%d", server_name, SERV_HOST_ADDR, tcp_port);
	write(sockfd, buffer, strlen(buffer));
	
	close(sockfd);
	pthread_detach(pthread_self());
	return NULL;
}

/*
Main function. Accepts client connections and spins them off into their 
own handler function.
*/
int main (int argc, char **argv){
	int 				sockfd, newsockfd, clilen, clients[MAX_CLIENTS];
	struct sockaddr_in 	cli_addr, serv_addr;
	pthread_t 			client_thread, directory_thread;

	// Check to ensure three args were passed. ./server "KSU Football" 23423
	if(argc != 3){
		my_error("Arguments should be the chat room name followed by the port. \n");
	}

	// Get the server name from the args. Store it for use with Part 2.
	strcpy(server_name, argv[1]);

	// Get the tcp port from args  and convert to int.
	tcp_port = atoi(argv[2]);

	// Zero out the active fd_set.
	FD_ZERO(&fds);

	// Spin off the directory handler thread which registers this chat server
	// with the directory server. 
	pthread_create(&directory_thread, NULL, &directory_handler, NULL);

	/*
	Code for zeroing out the client_names list. 
	Couldn't get working- assuming I need to malloc
	space for each new client name in the array of
	char pointers.

	for(i=0; i<MAX_CLIENTS; i++){
		client_names[i][0]='\0';
	}*/

	// Make TCP stream socket and get the file descriptor. 
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server cannot open stream socket");
		exit(1);
	}

	/* 
	Specify the port to use for the chat server which
	clients will use to access the server.
	*/
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(tcp_port);

	// Bind the master socket to the server address. 
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 ){
		perror("server: can't bind local address");
		exit(1);
	}
	
	// Start listening. I should be checking for errors, but not enought time.
	listen(sockfd, 5);

	// Put the master socket into the active fd_set. 
	FD_SET(sockfd, &fds);

	// Get the size of the client's address. 
	clilen = sizeof(cli_addr);

	/*
	Keep this loop going to accept new client connections. 
	 */
	for (;;) {
		// If a client is trying to connect, accept it. 
		newsockfd =  accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if(newsockfd <  0){
			my_error("server: accept error");
		}

		// Increment the number of clients.
		num_clients++;

		// If this is more than the max clients defined, drop the fd.
		if(num_clients >= MAX_CLIENTS){
			close(newsockfd);
			my_error("Max clients reached. Dropping connection.\n");
		}

		// Add the new client fd to the active fd_set.
		FD_SET(newsockfd, &fds);

		// Spin off a thread to deal with the new client!
		pthread_create(&client_thread, NULL, &client_handler, (void*)newsockfd);
	}

	// Put this here is the compiler chills out. 
	return(0);
}










