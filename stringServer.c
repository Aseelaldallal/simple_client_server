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
#define MAXUSERINPUTSIZE 2000


void printIPAddress(char *msg, struct sockaddr *anAddress); // Print IPv4 address in string form
void printMachineName(); // print name of machine server is running on
void printPortNumber(int aSocket); // print port number associated with aSocket
int receive(int clientSocket, char **msg);
int getMsgLength(int clientSocket);
int getMsg(int clientSocket, char * msgBuff, int numBytesToReceive);
char *unpack(char *msgWithLength, int headerSize, int totalLength);
int convertToTitleCase(char ** msg);
int sendAll(int sockfd, char * buff);

/* ---------------------------------------------------------------------- */
/* -------------------------------- MAIN -------------------------------- */
/* ---------------------------------------------------------------------- */

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
	
	fd_set master; // set of all file descriptors
	fd_set readfds; // file descriptors ready to read
	int fdmax; // Max file descriptor
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	
	status = listen(welcomeSocket, BACKLOG); 
	if(status == -1) {
		fprintf(stderr, "Server: failed to listen\n"); 
		exit(EXIT_FAILURE); 
	}
	
	// Add welcome socket to master
	FD_SET(welcomeSocket, &master);
	
	// keep track of the biggest file descriptor
    fdmax = welcomeSocket; // so far, it's this one
	
	    // main loop
    for(;;) {
        readfds = master; // copy it
        if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // run through the existing connections looking for data to read
        for(int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &readfds)) { 
                if (i == welcomeSocket) {
					printf("Incoming connection\n");
					int newSocket; // New connections
					struct sockaddr clientAddress; // client address information
					socklen_t clientAddressLen;
					clientAddressLen = sizeof clientAddress; 
					newSocket = accept(welcomeSocket, (struct sockaddr*)&clientAddress, &clientAddressLen); 
					if(newSocket == -1) {
						perror("Error on Accept");
						exit(EXIT_FAILURE); 
					}
					FD_SET(newSocket, &master); // add to master set
					if (newSocket > fdmax) {    // keep track of the max
						fdmax = newSocket;
					}
					printf("%d: Connection Established\n", newSocket); 
					printIPAddress("IP ", &clientAddress); 
                } else { // handle data from a client
					printf("Socket %d: Ready to read\n", i);
					char *msg;
                    int numBytes = receive(i, &msg); 
                    printf("Main loop: Num Bytes Recieved: %d\n", numBytes); 
					if( numBytes <= 0 ) {
						if( numBytes == -1 ) {
							perror("error on receive");
							exit(EXIT_FAILURE); 
						}
						printf("Closing Connection on socket %d\n", i); 
						close(i);
						FD_CLR(i, &master); 
					} else { // we got some data from a client
                        printf("%d: Got data from client\n", i); 
                        printf("%d msg: %s\n", i, msg);
                        printf("Length of msg: %lu\n", strlen(msg)); 
                        convertToTitleCase(&msg); 
                        printf("title case: %s\n", msg);
                        printf("Length of msg: %lu\n", strlen(msg)); 
                        sendAll(i, msg);
                        // need error checking here
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)

	// dont forget to close
	return 0;
	
}



/* ---------------------------------------------------------------------- */
/* ------------------------------- METHODS ------------------------------ */
/* ---------------------------------------------------------------------- */


/* Print IPv4 Address */
void printIPAddress(char *msg, struct sockaddr *anAddress) {
	char IPAddr[INET_ADDRSTRLEN]; 
	inet_ntop(anAddress->sa_family, &(((struct sockaddr_in*)anAddress)->sin_addr), IPAddr, sizeof IPAddr); 
	printf("%s: %s\n", msg, IPAddr);
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

/* Recieve Client Message. Returns 0 if client closed connection, -1 on error, length of msg received */
/*msg contains client message, not complete */
int receive(int clientSocket, char **msg) {
    int msgLength = getMsgLength(clientSocket); 
    printf("MsgLength: %d\n", msgLength); 
    if(msgLength == -1 || msgLength ==0) { return msgLength; }
    char msgWithLength[4 + msgLength];
    int totalReceived = getMsg(clientSocket, msgWithLength, 4 + msgLength);
    printf("TotalReceived: %d\n", totalReceived); 
    if (totalReceived == -1) { return -1; }
    if (totalReceived == 0 ) { printf("This is not supposed to happen!\n"); }
    //~ *msg = malloc(msgLength);  // free msg
    //~ printf("MsgLength: %d\n", msgLength); 
    //~ strncpy(*msg, unpack(msgWithLength, 4, 4+msgLength),msgLength); 
    //~ printf("In Recieve: This is msg: %s\n", *msg); 
    *msg = unpack(msgWithLength, 4, 4+msgLength);
    return totalReceived;
}

/* Unpacks: removes header from msgWithLength */
char *unpack(char *msgWithLength, int headerSize, int totalLength) {
	printf("---Unpack---\n"); 
	int textLength = totalLength - headerSize;
	printf("Text Length: %d\n", textLength); 
	char *msg = malloc(sizeof(char) * textLength);
	int j=0;
	for(int i=headerSize; i<totalLength;i++) {
		msg[j] = msgWithLength[i];
		printf("Msg[%d]: %c\n", j, msg[j]); 
		j++;
	}
	printf("IN unpack: size of msg: %lu\n", sizeof(msg));
	printf("IN unpack: len of msg: %lu\n", strlen(msg));
	printf("---Exit upack---\n"); 
	return msg;
}

/* Gets length of message*/
int getMsgLength(int clientSocket) {
	int n;
    int totalBytesExpected = 4;
    int bytesReceived = 0;
    int bytesRemaining = totalBytesExpected;
    char msgBuff[4];
	while( bytesReceived < totalBytesExpected ) {
		n = recv(clientSocket, msgBuff, totalBytesExpected, MSG_PEEK);
		if( n == -1 ) { return -1; } // Error
		if( n == 0  ) { return 0;  } // client closed connection
		bytesReceived += n;
		bytesRemaining -=n;
	}
	int msgLength;
	int *ml = &msgLength;
	memcpy(ml, msgBuff, 4); 
    return msgLength;
}

/* Receives length bytes from clientSocket. Stores them in msgBuff. Returns
 * the length. If flags set to MSG_PEEK, data is returned but not consumed
 * Returns -1 on error, 0 if client closed connection, msgLength otherwise */
int getMsg(int clientSocket, char * msgBuff, int numBytesToReceive) {
	int n;
    int totalBytesExpected = numBytesToReceive;
    int bytesReceived = 0;
    int bytesRemaining = totalBytesExpected;
    while( bytesReceived < totalBytesExpected ) {
		n = recv(clientSocket, msgBuff, numBytesToReceive, 0);
		if( n == -1 ) { return -1; } // Error
		if( n == 0  ) { return 0;  } // client closed connection - this shouldn't happen
		bytesReceived += n;
		bytesRemaining -=n;
	}
	return bytesReceived;
}


/* Converts  msg to title case */
int convertToTitleCase(char ** msg) {
	//*msg = "a";
	printf("In Convert to titlecase: length: %lu\n", strlen(*msg)); 
	for(int i=0; i<strlen(*msg); i++) {
	    //printf("Msg[%d]: %c\n", i, (*msg)[i]); 
	    if(i == 0 || (*msg)[i-1] == ' ') {
	        if( (*msg)[i] >= 'a' && (*msg)[i] <= 'z' ) {
				(*msg)[i] -= 32;
			}
	    } else if ((*msg)[i] >= 'A' && (*msg)[i] <= 'Z' ) {
			(*msg)[i] += 32;
		}
	}
	return 0;
}


// Send all of buff through sockfd
int sendAll(int sockfd, char * buff) {
	printf("IN SEND ALL"); 
	unsigned char textLengthBuffer[4];
	unsigned int textLength = strlen(buff);
	memcpy(textLengthBuffer, (char*)&textLength, 4); 
	int msgLength = 4 + textLength;
	char msg[msgLength];
	int i;
	for(i=0; i<4; i++) {
		msg[i] = textLengthBuffer[i];
	}
	int j=0;
	printf("Msg to send: "); 
	for(i=4; i<msgLength; i++) {
		msg[i] = buff[j++];
		printf("%c", msg[i]);
	}
	printf("\n"); 
	int n;
	int totalBytesToSend = sizeof(msg);
	int bytesSent = 0;
	int bytesRemaining = totalBytesToSend;
	while(bytesSent < totalBytesToSend) {
		n = send(sockfd, msg + bytesSent, bytesRemaining, 0);
		if( n == -1 ) { break; }
		bytesSent += n;
		bytesRemaining -=n;
	}
	return n==-1?-1:0;
}
