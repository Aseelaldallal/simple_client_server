
#include <pthread.h>
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
#include <sys/queue.h>

#define MAXUSERINPUTSIZE 2000 

/* ---------------------------------------------------------------------- */
/* ------------------------------ FUNCTIONS ----------------------------- */
/* ---------------------------------------------------------------------- */

void printIPAddress(char *msg, struct sockaddr *anAddress); // Print IPv4 address in string form
int sendAll(int sockfd, char * buff);
void * addUserInputToQueue();
void * sendToServer(void * sockfd);
void clean(char *var);

/* ---------------------------------------------------------------------- */
/* -------------------------- THREADS AND LOCKS ------------------------- */
/* ---------------------------------------------------------------------- */

pthread_mutex_t queueMutex;

/* ---------------------------------------------------------------------- */
/* -------------------------------- QUEUE ------------------------------- */
/* ---------------------------------------------------------------------- */

typedef struct BufferEntry {
	STAILQ_ENTRY(BufferEntry) next;
	char text[MAXUSERINPUTSIZE];
} BufferEntry;

typedef STAILQ_HEAD(BufferQueueHead, BufferEntry) BufferQueueHead;

BufferEntry *np, *prevp;
BufferQueueHead *bufferQHead;
	
/* ---------------------------------------------------------------------- */
/* -------------------------------- MAIN -------------------------------- */
/* ---------------------------------------------------------------------- */


int main()
{
	int status;

	/* --------------------- INIT QUEUE --------------------- */
	
	bufferQHead = malloc(sizeof(BufferQueueHead));
	STAILQ_INIT(bufferQHead); // empty list created
	
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
	
	
	pthread_t userInputHandlerThread, serverCommThread;
	status = pthread_create( &userInputHandlerThread, NULL, addUserInputToQueue, NULL);
	if (status != 0){
		printf("ERROR; return code from pthread_create() is %d\n", status);
        exit(EXIT_FAILURE);
    }
    
    status = pthread_create( &serverCommThread, NULL, sendToServer, &sockfd);
	if (status != 0){
		printf("ERROR; return code from pthread_create() is %d\n", status);
        exit(EXIT_FAILURE);
    }
    
	for(;;) { // forever
		

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

// Keep on getting user input
void * addUserInputToQueue() {
	char input[MAXUSERINPUTSIZE];
	while( fgets(input, MAXUSERINPUTSIZE, stdin) != NULL) {
		pthread_mutex_lock(&queueMutex); // lock
			np = malloc(sizeof(BufferEntry));
			strcpy(np->text, input); 
			STAILQ_INSERT_TAIL(bufferQHead, np, next);
		pthread_mutex_unlock(&queueMutex); //unlock
	}
	return 0;
}

// Keep on sending queue elements to server
void * sendToServer(void * sockfd) {
	for(;;) { // forever
		if(!STAILQ_EMPTY(bufferQHead)) {
			char textToSend[MAXUSERINPUTSIZE];
			clean(textToSend); 
			strncpy(textToSend, np->text, strlen(np->text));
			sendAll(*((int *)sockfd), textToSend);
			pthread_mutex_lock(&queueMutex); // lock
			STAILQ_REMOVE_HEAD(bufferQHead, next); 
			pthread_mutex_unlock(&queueMutex); //unlock
		}
	}
}


// Send all of buff through sockfd
int sendAll(int sockfd, char * buff) {
	
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
	for(i=4; i<msgLength; i++) {
		msg[i] = buff[j++];
	}
	
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




// Empties string
void clean(char *var) {
    int i = 0;
    while(var[i] != '\0') {
        var[i] = '\0';
        i++;
    }
}

