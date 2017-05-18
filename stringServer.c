/* Method: Convert to title case 
 * Print string received from client at the server
 * Use Select() to multiplex processing and connection requests from clients
 * Upperbound: 5 similtanous clients for a server
 * When server starts, it must print out two numbers: Port, Address
 * */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


#define PORT "0"
#define BACKLOG 5
#define MACHINENAMELENGTH 1024

// temp
void *get_in_addr(struct sockaddr *sa) {
  return sa->sa_family == AF_INET
    ? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
    : (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}



/* Prints name of computer server is running on */
void printMachineName() {
	char machineName[MACHINENAMELENGTH];
	if(gethostname(machineName, sizeof(machineName)) != 0) {
		perror("Can't get hostname"); 
		exit(EXIT_FAILURE);
	}
	printf("SERVER_ADDRESS %s\n", machineName);   // machine name
}

/* Prints port associated with aSocket */
void printPortNumber(int aSocket) {
	struct sockaddr_in sockAddressInfo;
    int sockAddressInfoLength;
    sockAddressInfoLength = sizeof(sockAddressInfo);
    if (getsockname(aSocket, (struct sockaddr *)&sockAddressInfo, &sockAddressInfoLength) == -1) {
       perror("getsockname() failed");
       exit(EXIT_FAILURE); 
    }
    printf("SERVER_PORT %d\n", (int) ntohs(sockAddressInfo.sin_port));
}


int main() {
	
	int status; 
	
	/* --------------- CREATE addrinfo STRUCT --------------- */
	
	struct addrinfo *serverInfo;
	struct addrinfo hints;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM; // tcp
	hints.ai_flags = AI_PASSIVE; // fill in IP address automatically
	
	status = getaddrinfo(NULL, PORT, &hints, &serverInfo); 
	if(status != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}
	
	/* --------------- CREATE SOCKET AND BIND --------------- */
	
	int welcomeSocket; 
	
	// serverInfo points to a linked list of addrinfo structs
	// loop through all the results and bind to the first we can
	struct addrinfo *looper; // to traverse linked list
	for (looper = serverInfo; looper != NULL; looper = looper->ai_next) {
		// Attempt creating socket
		welcomeSocket = socket(looper->ai_family, looper->ai_socktype, looper->ai_protocol);
		if(welcomeSocket == -1) {
			continue;
		}
		// Attempt binding
		status = bind(welcomeSocket, looper->ai_addr, looper->ai_addrlen);
		if(status == -1) {
			continue;
		}
		break; // If we got here, then successful socket creation and bind. Exit loop
	}
	
	// At this point welcomeSocket should be a file descriptor, and should be bound to an
	// IP and Port. If not, exit with error
	if(looper == NULL) { // Couldn't create socket or bind
		fprintf(stderr, "server: failed to bind\n"); 
		exit(EXIT_FAILURE); 
	}
	
	freeaddrinfo(serverInfo); // Free linked list. 
	
	printMachineName(); 
	printPortNumber(welcomeSocket); 
	
	/* ------------------------ LISTEN ---------------------- */
	
	status = listen(welcomeSocket, BACKLOG); 
	if(status == -1) {
		fprintf(stderr, "Server: failed to listen\n"); 
		exit(EXIT_FAILURE); 
	}
	
		
	/* ------------------------ ACCEPT ---------------------- */
	
	int newSocket; // New connections
	struct sockaddr_storage clientAddress; // client address information
	socklen_t clientAddressLen;
	char IPAddr[INET_ADDRSTRLEN]; 
	
	while(1) { // Accept Loop
		clientAddressLen = sizeof clientAddress; 
		newSocket = accept(welcomeSocket, (struct sockaddr*)&clientAddress, &clientAddressLen); 
		if(newSocket == -1) {
			perror("Error on Accept");
			exit(EXIT_FAILURE); 
		}
		inet_ntop(clientAddress.ss_family, get_in_addr((struct sockaddr *)&clientAddress), IPAddr, sizeof IPAddr);
        printf("server: got connection from %s\n", IPAddr);
	}
	
	/* --------------------- SEND AND RECV ------------------ */




	/* ------------------------- CLOSE ---------------------- */
	


	return 0;
	
}



