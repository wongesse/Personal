#define BUFFER_LENGTH 256

#include <iostream>

#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <string.h>
#include "interface.h"

using namespace std;

int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    
	while (1) {
	
		int sockfd = connect_to(argv[1], atoi(argv[2]));
		char command[MAX_DATA];
        get_command(command, MAX_DATA);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		}
	
		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	int sockfd = -1;
	struct sockaddr_in serverAddr;
	memset (&serverAddr, 0, sizeof(serverAddr));

    // Init new socket for client
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\nERROR: Socket creation error\n");
		return -1;
	}

    // Specify host and port
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(host);
	serverAddr.sin_port = htons(port);

    // Attempt connection to server
	if (connect(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
		printf("\nERROR: Connection failed \n");
		return -1;
	}

	return sockfd;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
    char buf[BUFFER_LENGTH];
	send(sockfd, command, BUFFER_LENGTH, 0);
	recv(sockfd, buf, BUFFER_LENGTH, 0);

	int compare; 

    // Split reply string into parts to insert into reply object
	char * status = strtok(buf, " ");
	char * num_member = strtok(NULL, " ");
	char * port = strtok(NULL, " ");
	char * list_room = strtok(NULL, " ");

	struct Reply reply;

    // Parse status
	if ((compare = strcmp(status, "SUCCESS\0")) == 0) {
		reply.status = SUCCESS;
	} else if ((compare = strcmp(status, "FAILURE_ALREADY_EXISTS")) == 0) {
		reply.status = FAILURE_ALREADY_EXISTS;
	} else if ((compare = strcmp(status, "FAILURE_NOT_EXISTS")) == 0) {
		reply.status = FAILURE_NOT_EXISTS;
	} else if ((compare = strcmp(status, "FAILURE_INVALID")) == 0) {
		reply.status = FAILURE_INVALID;
	} else if ((compare = strcmp(status, "FAILURE_UNKNOWN")) == 0) {
		reply.status = FAILURE_UNKNOWN;
	}

    // Parse other reply variables
	if (num_member != NULL) {
		reply.num_member = atoi(num_member);
	}
	if (port != NULL) {
		reply.port = atoi(port);		
	}
	if (list_room != NULL) {
		strcpy(reply.list_room, list_room);
	}

	return reply;
}

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	int chatroom, activity;
	char buffer[BUFFER_LENGTH];
	fd_set rfds, afds;

	chatroom = connect_to(host, port);
    if (chatroom == -1) {
        cout << "Chatroom does not exist." << endl;
        return;
    }

	FD_ZERO(&afds);
    FD_ZERO(&rfds);

    // Place stdin and the chatroom socket into the fd_set
	FD_SET(chatroom, &afds);
	FD_SET(0, &afds);

    // A dummy message is sent to the server to ensure there are no dropped messages
    string message = "";
    send(chatroom, message.c_str(), BUFFER_LENGTH, 0);

	while(1) {
        memcpy(&rfds, &afds, sizeof(rfds));
		if ((activity = select(chatroom+1, &rfds, 0, 0 ,0)) < 0) {
			cout << "ERROR: could not select" << endl;
		}

        // Monitor for activity on chatroom and stdin and decide how to handle it
		if (FD_ISSET(chatroom,&rfds)) {
			if ((activity = recv(chatroom, buffer, BUFFER_LENGTH, 0)) == 0) {
                cout << "Server has been killed. Shutting down chatmode." << endl;
                close(chatroom);
                FD_ZERO(&rfds);
                FD_ZERO(&afds);
                return;
            }
            if (strcmp(buffer, "") == 0) {
                continue;
            }
			printf("> %s", buffer);
		}
		if (FD_ISSET(0, &rfds)) {
            fgets(buffer, MAX_DATA, stdin);
			send(chatroom, buffer, BUFFER_LENGTH, 0);
        }
		memset(buffer, 0 , BUFFER_LENGTH);	
	}
}
