
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
void * sendAndReceive(void * sockfd);
void clean(char *var);
int receive(int clientSocket, char **msg);
int getMsgLength(int clientSocket);
int getMsg(int clientSocket, char * msgBuff, int numBytesToReceive);
char *unpack(char *msgWithLength, int headerSize, int totalLength);

/* ---------------------------------------------------------------------- */
/* -------------------------- THREADS AND LOCKS ------------------------- */
/* ---------------------------------------------------------------------- */

pthread_mutex_t queueMutex;

/* ---------------------------------------------------------------------- */
/* -------------------------------- QUEUE ------------------------------- */
/* ---------------------------------------------------------------------- */

typedef struct BufferEntry {
	STAILQ_ENTRY(BufferEntry) entries;
	char text[MAXUSERINPUTSIZE];
} BufferEntry;

typedef STAILQ_HEAD(BufferQueueHead, BufferEntry) BufferQueueHead;

BufferEntry *np;
BufferQueueHead *bufferQHead;
	
/* ---------------------------------------------------------------------- */
/* ---------------------------- OTHER GLOBAL ---------------------------- */
/* ---------------------------------------------------------------------- */

int numMsgsToSend = 0;
int numMsgsRecieved = 0;

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

    freeaddrinfo(serverInfo); 

	/* --------------------- SEND AND RECV ------------------ */
	
	
	pthread_t userInputHandlerThread, serverCommThread;
	status = pthread_create( &userInputHandlerThread, NULL, addUserInputToQueue, &sockfd);
	if (status != 0){
		printf("ERROR; return code from pthread_create() is %d\n", status);
        exit(EXIT_FAILURE);
    }
    
    status = pthread_create( &serverCommThread, NULL, sendAndReceive, &sockfd);
	if (status != 0){
		printf("ERROR; return code from pthread_create() is %d\n", status);
        exit(EXIT_FAILURE);
    }
    
	for(;;) {}
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
void * addUserInputToQueue(void * sockfd) {
	char input[MAXUSERINPUTSIZE];
	while( fgets(input, MAXUSERINPUTSIZE, stdin) != NULL) {
		strtok(input, "\n"); // remove newline char
		if(strlen(input) == 1 && input[0] == '\n') {
			continue; // dont send an empty line
		}
		pthread_mutex_lock(&queueMutex); // lock
			np = malloc(sizeof(BufferEntry));
			strcpy(np->text, input); 
			STAILQ_INSERT_TAIL(bufferQHead, np, entries);
			numMsgsToSend++;
		pthread_mutex_unlock(&queueMutex); //unlock
	}
	// Get here when fgets is null
    while(numMsgsToSend!=numMsgsRecieved) {} // do nothing 
	close(*((int *)sockfd)); 
	return 0;
}

// Keep on sending queue elements to server
void * sendAndReceive(void * sockfd) { 
	for(;;) { // forever
		pthread_mutex_lock(&queueMutex); // lock
			int isEmpty = STAILQ_EMPTY(bufferQHead); // Returns 1 if empty, 0 if not empty. 
		pthread_mutex_unlock(&queueMutex); //unlock
		if(isEmpty == 0) { // if queue is not empty
			char textToSend[MAXUSERINPUTSIZE];
			clean(textToSend); 
			pthread_mutex_lock(&queueMutex); // lock
				BufferEntry *first= STAILQ_FIRST(bufferQHead);
				strncpy(textToSend, first->text, strlen(np->text));
				sendAll(*((int *)sockfd), textToSend);
				STAILQ_REMOVE_HEAD(bufferQHead, entries); 
				free(first); 
			pthread_mutex_unlock(&queueMutex); //unlock
			int n;
			char msgBuff[4];
			n = recv(*((int *)sockfd), msgBuff, 4, MSG_PEEK);
			if( n > 0 ) {
				char *msg;
				receive(*((int *)sockfd), &msg); 
				numMsgsRecieved++;
				printf("Server: %s\n", msg); 
			} 
			sleep(2);
		} // End if
	} // End for
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

/* Recieve Client Message. Returns 0 if client closed connection, -1 on error, length of msg received */
/*msg contains client message, not complete */
int receive(int clientSocket, char **msg) {
    int msgLength = getMsgLength(clientSocket); 
    if(msgLength == -1 || msgLength ==0) { return msgLength; }
    char msgWithLength[4 + msgLength];
    int totalReceived = getMsg(clientSocket, msgWithLength, 4 + msgLength);
    if (totalReceived == -1) { return -1; }
    if (totalReceived == 0 ) { printf("This is not supposed to happen!\n"); }
    *msg = unpack(msgWithLength, 4, 4+msgLength);
    return totalReceived;
}

/* Unpacks: removes header from msgWithLength */
char *unpack(char *msgWithLength, int headerSize, int totalLength) {
	int textLength = totalLength - headerSize;
	char *msg = malloc(sizeof(char) * textLength);
	int j=0;
	for(int i=headerSize; i<totalLength;i++) {
		msg[j] = msgWithLength[i];
		j++;
	}
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
