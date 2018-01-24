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

typedef struct sinfo sinfo;
static pthread_mutex_t lock;

// My custom struct for each chat server.
struct sinfo {
    char* name;
    char* address;
    char* port;
};

// Global list of active chat servers. 
static sinfo *master_list_servers[MAX_CLIENTS];

/*
Mallocs space for a new sinfo struct and 
mallocs space for each property. Returns
a pointer to the struct. 
*/
struct sinfo *new_server_info () {

    // Try to allocate server info structure.
    sinfo *retVal = malloc (sizeof (sinfo));
    if (retVal == NULL)
        return NULL;

    // Try to allocate vector data, free structure if fail.

    retVal->name = malloc (sizeof (char*));
    if (retVal->name == NULL) {
        free (retVal);
        return NULL;
    }

    retVal->address = malloc (sizeof (char*));
    if (retVal->address == NULL) {
        free (retVal);
        return NULL;
    }

    retVal->port = malloc (sizeof (char*));
    if (retVal->port == NULL) {
        free (retVal);
        return NULL;
    }

    return retVal;
}

// Frees space malloced for a sinfo.
void delete_server_info (sinfo *info) {
    if (info != NULL) {
        free (info->name);
        free (info->address);
        free (info->port);
        free (info);
    }
}

// Prints all the active chat servers. Used internally. 
void print_master_list(){
	for(int i = 0; i < MAX_CLIENTS; i++){
		if(master_list_servers[i]){
			sinfo* temp_info = master_list_servers[i];
			printf("%s, %s, %s\n", temp_info->name, temp_info->address, temp_info->port );
		}
	}
}


/*
Listens for incoming client connections and 
prints out all the active servers to the client.
*/
void *client_handler(){
	int clientfd, newsockfd;
	struct sockaddr_in	cli_addr;
	char out_buffer[MAX*10], in_buffer[MAX];
	int readin, clilen;
	char response[5];
	

	if ( (clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		my_error("directory: cannot open stream socket");
	}

	memset((char *) &cli_addr, 0, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = INADDR_ANY;
	cli_addr.sin_port = htons(DIRECTORY_CLIENT_PORT);


	if (bind(clientfd, (struct sockaddr *) &cli_addr, sizeof(cli_addr)) < 0 ){
		my_error("directory: can't bind local address");
	}

	if((listen(clientfd, 5)) < 0){
		my_error("directory: can't listen! ");
	}

	clilen = sizeof(cli_addr);

	for(;;){
		out_buffer[0] = '\0';
		newsockfd =  accept(clientfd, (struct sockaddr *) &cli_addr, &clilen);

		// If the sockfd was successful, lock the master server list.
		if(newsockfd > 0){
			// WRITE THE GLOBAL LIST OF DIRECTORIES
			pthread_mutex_lock(&lock);
			int len = 0;

			// Format all the active chat servers. 
			for(int i =0; i < MAX_CLIENTS; i++){
				if(master_list_servers[i]){
					sinfo* temp_info = master_list_servers[i];
					len += sprintf(out_buffer + len, "%d. %s, %s, %s\n", i+1, temp_info->name, temp_info->address, temp_info->port );
				}
			}
			pthread_mutex_unlock(&lock);


			// Write out the active chat servers. 
			write(newsockfd, out_buffer, strlen(out_buffer));

			// Wait for the client response.
			readin = read(newsockfd, response, sizeof(response));

			memset(out_buffer, 0, MAX*10);

			// Cast the response to an int.
			int choice = response[0] - '0';

			// Format and send back the address,port string for the 
			// client to connect to.
			sinfo* temp_info = master_list_servers[choice-1];
			sprintf(out_buffer, "%s,%s", temp_info->address, temp_info->port );
			write(newsockfd, out_buffer, strlen(out_buffer));
		}
		else{
			my_error("directory: client accept error");
		}
		close(newsockfd);
	}

}

/*
Adds a new server info struct to the master server list
of active chat servers.
*/
void add_to_master(char* name, char* address, char* port){
  	
  	// Malloc some new space for the server info.
  	sinfo* new_info = new_server_info();
	
	// Malloc space for each property. 
	new_info->name = malloc(strlen(name));
	strcpy(new_info->name, name);

	new_info->address = malloc(strlen(address));
	strcpy(new_info->address, address);

	new_info->port=malloc(strlen(port));
	strcpy(new_info->port, port);

	// Grab the lock and find a space to put the new server info.
	pthread_mutex_lock(&lock);
	for(int i = 0; i < MAX_CLIENTS; i++){
		if(!master_list_servers[i]){
			master_list_servers[i] = new_info;
			break;
		}
	}
	pthread_mutex_unlock(&lock);
}

/*
Manages a list of active chat servers, sending the list
to incoming clients, and adding new server infos as
new servers connect.
*/
int main(int argc, char **argv){
	int serverfd, new_server_fd, clilen;
	int readinlen;
	struct sockaddr_in	serv_addr;
	pthread_t client_thread;
	char in_buffer[MAX];
	

	if (pthread_mutex_init(&lock, NULL) != 0)
    {
        my_error("\n mutex init failed\n");
    }

    for(int i = 0; i < MAX_CLIENTS; i++){
    	master_list_servers[i] = NULL;
    }

    // Client thread to send the active chat servers to any clients
    // that connect. Spin off a thread to listen on the client port.
	pthread_create(&client_thread, NULL, &client_handler, NULL);

	if ( (serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		my_error("directory: cannot open stream socket");
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(DIRECTORY_SERVER_PORT);


	if (bind(serverfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0 ){
		my_error("directory: can't bind local address");
	}

	if((listen(serverfd, 5)) < 0){
		my_error("directory: can't listen! ");
	}

	clilen = sizeof(serv_addr);

	for(;;){

		// Accept new server connections.
		new_server_fd =  accept(serverfd, (struct sockaddr *) &serv_addr, &clilen);
		if(new_server_fd > 0){
			memset(in_buffer, 0, MAX);

			// Read in the message from the new server giving us
			// the server's name, address, and port.
			readinlen = read(new_server_fd, in_buffer, MAX);
			
			//in_buffer[readinlen] = '\0';
	
			// If nothing in the in buffer, go ahead and keep reading.
			if(!strlen(in_buffer)){
				continue;
			}

			// Parse the name, address, and port from what
			// we got from the read call.
			char *name, *address, *port;
			name = strtok(in_buffer,",");
			address = strtok(NULL, ",");
			port = strtok(NULL, ",");

			// Add this server to the list of active servers.
			add_to_master(name, address, port);
		}
		else{
			my_error("directory: client accept error");
		}

		close(new_server_fd);
	}

return 0;

}