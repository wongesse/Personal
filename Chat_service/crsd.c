#define BUFFER_LENGTH 256
#define SERVER_PORT 8000
#define BEGIN_PORT 3000

#include <iostream>
#include <vector>
#include <string>
#include <map>

#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#include <string.h>

using namespace std;

// static status message strings
static string SUCCESS = "SUCCESS";
static string FAILURE_ALREADY_EXISTS = "FAILURE_ALREADY_EXISTS";
static string FAILURE_NOT_EXISTS = "FAILURE_NOT_EXISTS";
static string FAILURE_INVALID = "FAILURE_INVALID";
static string FAILURE_UNKNOWN = "FAILURE_UNKNOWN";

// Chatroom structure in an array of uninitialized chatrooms
struct Chatroom {
    int socket;
    int port;
    int num_member;
    string name;
    pthread_t * thread;
    vector<int> clients;
};

// Map to hold pointers to chatrooms
map<string, struct Chatroom *>* chatrooms;

// Chatrooms start at port 3000 and increase as new ones are created.
static int chatroom_port = BEGIN_PORT;

// Function declarations
int passiveTCPConnection(int PORT);
void process_command(int sd, char * buffer);
void create_chatroom(int sd, char * chatroom_name);
void join_chatroom(int sd, char * chatroom_name);
void list_chatrooms(int sd);
void delete_chatroom(int sd, char * chatroom_name);
void * manage_chatroom(void * arg);

int main(int argc, char** argv) 
{
	int server_socket, new_socket, activity, valread, nfds;
	fd_set rfds, afds;
    char buffer[BUFFER_LENGTH];

    // Initialize the map of chatrooms
    chatrooms = new map<string, struct Chatroom*>();

    // Retrieve a server socket
    server_socket = passiveTCPConnection(SERVER_PORT);	

    nfds = getdtablesize();

	FD_ZERO(&afds);
	FD_SET(server_socket, &afds);

	while(1) {
        // Copy afds into rfds for the loop iteration
		memcpy(&rfds, &afds, sizeof(rfds));

        // Wait for activity on all set sockets
		if ((activity = select(nfds, &rfds, 0, 0, 0)) < 0) {
			printf("ERROR: Select error\n");
		}

        // Check for data on the master socket
		if (FD_ISSET(server_socket, &rfds)) {  
            // Create new socket by accepting new connection
			if ((new_socket = accept(server_socket, NULL, NULL)) < 0) {
				printf("ERROR: Could not accept\n");
			}
			
            FD_SET(new_socket, &afds);
		}

        // Iterate through all socket descriptors and check for data
		for (int fd = 0; fd < nfds; fd++) {

            // Check for data on all sockets except master
			if (fd != server_socket && FD_ISSET(fd, &rfds)) {
                memset(buffer, 0, BUFFER_LENGTH);

                // This conditional checks to see if a client has disconnected.
	            if ((activity = recv(fd, buffer, BUFFER_LENGTH, 0) == 0)) {
                    close(fd);
                    FD_CLR(fd, &afds);
                    continue;
                }
                process_command(fd, buffer);
                close(fd);
                FD_CLR(fd, &afds);
			}

		}
	}	

}

/*
 * Establishes a server-side connection on the given port
 * 
 * @parameter PORT  Port to create new socket on
 */
int passiveTCPConnection(int PORT) 
{
    int server_socket;
    struct sockaddr_in serverAddr;
    
    // Begin setting up server connection
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Create the server's socket
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		printf("ERROR: Socket failed");
		return -1;
	}

    // Bind the socket
	if (bind(server_socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
		printf("ERROR: Bind failed\n");
		return -1;
	}

    // Listen for connections
	if (listen(server_socket, 10) < 0) {
		printf("ERROR: Can't listen");
		return -1;
	}

    return server_socket;
}

/*
 * Function takes command from client and choose how to interpret it.
 * 
 * @parameter sd        Master socket of the server
 * 
 * @parameter buffer    Message received by connected client
 * 
 */
void process_command(int sd, char * buffer) 
{

	int activity, res;
	char * command;
	char * chatroom_name;

    // Command Format: <COMMAND> <OPTIONAL CHATROOM PARAM>
	command = strtok(buffer, " "); // Should never be NULL
	chatroom_name = strtok(NULL, " "); // Could possibly be NULL if command is LIST
    for (int i = 0; command[i]; i++) command[i] = toupper((unsigned char)command[i]);


    // Received an empty message from client. Handling error by returning.
    if (command == NULL) {
        return;
    }

    // Determine command
	if ((res = strcmp(command, "CREATE")) == 0) {
		create_chatroom(sd, chatroom_name);
	}
	else if ((res = strcmp(command, "JOIN")) == 0) {
		join_chatroom(sd, chatroom_name);
	}
    else if ((res = strcmp(command, "LIST")) == 0) {
		list_chatrooms(sd);
	}
	else if ((res = strcmp(command, "DELETE")) == 0) {
	    delete_chatroom(sd, chatroom_name);

	} 
    else {
        string message = "FAILURE_UNKNOWN";
        char buffer[BUFFER_LENGTH];
        strcpy(buffer, message.c_str());
        send(sd, buffer, BUFFER_LENGTH, 0);
    }
	
}

/*
 * Function creates new chatroom.
 * 
 * @parameter sd             Master socket of the server
 * 
 * @parameter chatroom_name  Name of chatroom to create
 */
void create_chatroom(int sd, char * chatroom_name) 
{

    char buffer[BUFFER_LENGTH];

    // Check to see if a chatroom with the given name exists
    for( auto const& it : (*chatrooms) ) {
        if (strcmp(it.first.c_str(), chatroom_name) == 0) {
            // Chatroom already exists, inform the client and return
            strcpy(buffer, FAILURE_ALREADY_EXISTS.c_str());
            send(sd, buffer, BUFFER_LENGTH, 0);
            return;
        }
    }

    // Set up new socket here.
    int chatroom_socket = passiveTCPConnection(chatroom_port);

    // Confirmed that chatroom does not exist, so populate new room
    struct Chatroom * chat = new Chatroom();
    chat->socket = chatroom_socket;
    chat->port = chatroom_port;
    chat->num_member = 0;
    chat->name = string(chatroom_name);
    (*chatrooms)[chatroom_name] = chat;

    chatroom_port++;

    // Format return string and send to client
    string message = SUCCESS + " 0 " + to_string(chat->port);
    strcpy(buffer, message.c_str());
    send(sd, buffer, BUFFER_LENGTH, 0);

    // Thread will be the running chatroom. The thread will be passed the 
    pthread_t chatroom_thread;
    chat->thread = &chatroom_thread;

    // Launch a thread to manage the chatroom
    int errcode = pthread_create(&chatroom_thread, NULL, manage_chatroom, (void*)chat);
    if (errcode != 0) {
        cout << "Error creating thread with code: " << errcode << endl;
    }
}

/*
 * Function joins client to chatroom and returns port of chatroom to client
 * 
 * @parameter sd             Master socket of the server
 * 
 * @parameter chatroom_name  Name of chatroom to join
 */
void join_chatroom(int sd, char * chatroom_name) 
{

    string reply_string = "";

    // First, check to see if chatroom actually exists
	auto it = (*chatrooms).find(string(chatroom_name));
	if (it == (*chatrooms).end()) {
		reply_string = "FAILURE_NOT_EXISTS -1 -1";
	}
	else {
        struct Chatroom * c = (*chatrooms)[string(chatroom_name)];
		c->num_member += 1;
		reply_string = "SUCCESS " + to_string(c->num_member) + " " + to_string(c->port);
	}

	send(sd, reply_string.c_str(), BUFFER_LENGTH, 0);

}

/*
 * Function lists all created chatrooms.
 * 
 * @parameter sd    Master socket of the server
 */
void list_chatrooms(int sd) 
{
	string reply_string = SUCCESS + " 0 0 ";

    // Iterate through map and return names of each chatroom
	for (auto const& it : (*chatrooms)) {
		reply_string = reply_string + it.first + ",";
	}

	send(sd, reply_string.c_str(), BUFFER_LENGTH, 0);

}

/*
 * Function deletes chatroom and informs clients
 * 
 * @parameter sd             Master socket of the server
 * 
 * @parameter chatroom_name  Name of chatroom to delete
 */
void delete_chatroom(int sd, char * chatroom_name) 
{

    string reply_string = "";

    // First, check to see if chatroom actually exists
    auto it = (*chatrooms).find(string(chatroom_name));
	if (it == (*chatrooms).end()) {
		reply_string = "FAILURE_NOT_EXISTS";
	} else {
        // Send a message about chatroom closing to each member of room
        struct Chatroom * c = (*chatrooms)[string(chatroom_name)];
        for (int& fd : c->clients) {
            string message = "chat room being deleted\n";
            send(fd, message.c_str(), BUFFER_LENGTH, 0);
            close(fd);
        }

        // Kill the thread running the chatroom
        pthread_cancel(*c->thread);
        (*chatrooms).erase(chatroom_name);
        reply_string = SUCCESS + " 0 0";

    }

    send(sd, reply_string.c_str(), BUFFER_LENGTH, 0);

}

/*
 * This function is used to manage the chatroom threads, by accepting
 * new clients and relaying messages between clients.
 */
void * manage_chatroom(void * arg) 
{
    struct Chatroom * chat = (struct Chatroom *)arg;
    int new_socket, activity;
    fd_set rfds, afds;
    char buffer[BUFFER_LENGTH];

    // Clear out afds and then set the master socket into afds
	FD_ZERO(&afds);
	FD_SET(chat->socket, &afds);

    int nfds = getdtablesize();
    
    while(1) {
        // Copy afds into rfds for the loop iteration
		memcpy(&rfds, &afds, sizeof(rfds));

        // Wait for activity on all set sockets
		if ((activity = select(nfds, &rfds, 0, 0, 0)) < 0) {
			printf("ERROR: Select error\n");
            break;
		}

        // Check for data on the master socket
		if (FD_ISSET(chat->socket, &rfds)) {  
            // Create new socket by accepting new connection
			if ((new_socket = accept(chat->socket, NULL, NULL)) < 0) {
				printf("ERROR: Could not accept\n");
			}

            chat->clients.push_back(new_socket);
			
            // Set new socket into afds
            FD_SET(new_socket, &afds);
            FD_SET(new_socket, &rfds);
		}

        // Iterate through all socket descriptors and check for data
		for (int i = 0; i < chat->clients.size(); i++) {

            // Check for data on all sockets except master
			if (chat->clients[i] != chat->socket && FD_ISSET(chat->clients[i], &rfds)) {
                // Grab the data from the client. Check if the socket has closed.
                memset(buffer, 0, BUFFER_LENGTH);
	            if ((activity = recv(chat->clients[i], buffer, BUFFER_LENGTH, 0) == 0)) {
                    chat->clients.erase(chat->clients.begin() + i);
                    chat->num_member--;
                    close(chat->clients[i]);
                    FD_CLR(chat->clients[i], &afds);
                    continue;
                }

                
                // Relay this received message to all connected clients
                for (int j = 0; j < chat->clients.size(); j++) {
                    if (chat->clients[j] != chat->socket && chat->clients[j] != chat->clients[i]) {
                        send(chat->clients[j], buffer, BUFFER_LENGTH, 0);
                    }
                }
			}

		}

	}

}