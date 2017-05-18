

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 

void printIPAddress(char *msg, struct sockaddr *anAddress); // Print IPv4 address in string form
int sendAll(int sockfd, char * buff, int * len);

/* ---------------------------------------------------------------------- */
/* -------------------------------- MAIN -------------------------------- */
/* ---------------------------------------------------------------------- */


int main()
{
	int status;

	/* --------------- CREATE addrinfo STRUCT --------------- */
	
	struct addrinfo hints, *serverInfo, *looper;
	    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP

    if ((status = getaddrinfo(getenv("SERVER_ADDRESS"), getenv("SERVER_PORT"), &hints, &serverInfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE); 
    }

	/* ---------------------- CONNECT ----------------------- */

	int sockfd;
	
	// serverInfo is linked list of addrinfo structs
    // traverse linked list and connect to the first we can
    for(looper = serverInfo; looper != NULL; looper = looper->ai_next) {
        if ((sockfd = socket(looper->ai_family, looper->ai_socktype, looper->ai_protocol)) == -1) {
            continue;
        }
        if (connect(sockfd, looper->ai_addr, looper->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (looper == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(EXIT_FAILURE);
    }

	printIPAddress("Server IP", (struct sockaddr *)looper->ai_addr); 

    freeaddrinfo(serverInfo); 

	/* --------------------- SEND AND RECV ------------------ */
	
	char buffer[1024];
	while( fgets(buffer, 1024, stdin) != NULL) {
		int len = sizeof buffer;
		if( sendAll(sockfd, buffer, &len) == -1) {
			printf("Only sent %d bytes because of error\n", len);
		} 
	}


	/* ------------------------- CLOSE ---------------------- */

    close(sockfd);

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



// Send all of buff through sockfd
int sendAll(int sockfd, char * buff, int * len) {
	int n;
	int totalBytesToSend = strlen(buff);
	int bytesSent = 0;
	int bytesRemaining = totalBytesToSend;
	while(bytesSent < totalBytesToSend) {
		n = send(sockfd, buff + bytesSent, bytesRemaining, 0);
		if( n == -1 ) { break; }
		bytesSent += n;
		bytesRemaining -=n;
	}
	*len = bytesSent; // holds total number bytes sent
	return n==-1?-1:0;
}
