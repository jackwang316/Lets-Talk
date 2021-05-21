#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "list.h"

#define LINE_LENGTH 4000 * sizeof(char)
#define ENCRYPTION_KEY 12
#define MAX_BUFFER 1024

bool EXIT_FLAG = false;
bool REMOTE_STATUS = false;
int buffer[MAX_BUFFER];
sem_t empty;
sem_t full;
pthread_t thread1;
pthread_t thread2;
pthread_t thread3;
pthread_t thread4;
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

char *toFree;
List *inputList;
List *outputList;

typedef struct ReceiverArguments
{	
	int socket_fd;
	struct sockaddr_in addr;
}ReceiverArguments;

typedef struct SenderArguments
{
	int socket_fd;
	struct sockaddr_in addr;
}SenderArguments;

void printWelcome();
void sender(SenderArguments *args);
void sendMsg(List* inputList, int socket_fd, struct sockaddr_in dest);
void Inputer();
void getOutput();
void encrypt(char* input);
void receiver(ReceiverArguments *args);
void receiveMsg(List* outputList, int socket_fd, struct sockaddr_in src);
void printStatus();
void getInput();
void decrypt(char* input);

int main(int argc, char const *argv[])
{	

	if(argc < 4){
		printf("%s\n", "Incorrect amount of arguments.");
		exit(1);
	}
	int fd;
	if((fd= socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("%s\n", "Socket creation error");
		exit(1);
	}

	struct sockaddr_in hostAddr, remoteAddr;
	memset(&hostAddr, 0, sizeof(hostAddr));
	memset(&remoteAddr, 0, sizeof(remoteAddr));

	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = htons(strtoul(argv[3], NULL, 0));	
	remoteAddr.sin_addr.s_addr = inet_addr(argv[2]);

	hostAddr.sin_family = AF_INET;
	hostAddr.sin_port = htons(strtoul(argv[1], NULL, 0));
	hostAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(fd, (struct sockaddr *)(&hostAddr), sizeof(hostAddr)) < 0){
		printf("%s\n", "Socket binding failure");
		exit(1);
	}
	inputList = List_create();
	outputList = List_create();

	printWelcome();
	struct ReceiverArguments ra;
	struct SenderArguments sa;

	memset(&ra, 0, sizeof(ra));
	memset(&sa, 0, sizeof(sa));
	ra.socket_fd = fd;
	ra.addr = remoteAddr;

	sa.socket_fd = fd;
	sa.addr = remoteAddr;
	pthread_create(&thread1, NULL, (void *) &Inputer, NULL);
	pthread_create(&thread2, NULL, (void *) &sender, &sa);
	pthread_create(&thread3, NULL, (void *) &receiver, &ra);
	pthread_create(&thread4, NULL, (void *) &getOutput, NULL);

	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);
	pthread_join(thread3,NULL);
	pthread_join(thread4,NULL);

	free(toFree);
	List_free(inputList, NULL);
	List_free(outputList, NULL);
	pthread_exit(NULL);
	exit(0);
}

void printWelcome(){
	printf("%s\n", "Welcome to LetS-Talk! Please type your messages now.");
}

void getOutput(){
	while(EXIT_FLAG != true){
		if(List_count(outputList) > 0){
			fflush(stdout);
			char *temp = malloc(LINE_LENGTH);
			memset(temp, 0, LINE_LENGTH);
			pthread_mutex_lock(&m2);
			strcpy(temp, (char *)List_trim(outputList));
			pthread_mutex_unlock(&m2);
			printf("%s", temp);
			free(temp);
		}
	}
}

void Inputer(){
	while(EXIT_FLAG != true){
		char *input = malloc(LINE_LENGTH);
		toFree = input;
		memset(input, 0, LINE_LENGTH);
		fgets(input, LINE_LENGTH, stdin);
		fflush(stdin);
		if(strstr(input, "!status") != NULL){
			printStatus();
		}
		pthread_mutex_lock(&m1);
		int n = List_prepend(inputList, input);
		pthread_mutex_unlock(&m1);
		if(n == LIST_FAIL){
			printf("Failed to add to input list\n");
		}
	}
}

void receiver(struct ReceiverArguments *args){
	while(EXIT_FLAG != true){
		receiveMsg(outputList, (int) args->socket_fd, args->addr);
	}
	pthread_cancel(thread1);
}

void printStatus(){
	if(REMOTE_STATUS == true){
		printf("%s\n", "Online");
	}else{
		printf("%s\n", "Offline");
	}
}

void receiveMsg(List* outputList, int socket_fd, struct sockaddr_in src){
	char *buff = malloc(LINE_LENGTH);
	int n, len;
	if(EXIT_FLAG != true){
		n = recvfrom(socket_fd, buff, LINE_LENGTH, MSG_DONTWAIT, (struct sockaddr *) &src, &len);
		if(n > 0){
			REMOTE_STATUS = true;
			buff[n]  = '\0';
			decrypt(buff);
			if(strstr(buff, "!status") != NULL){
				memset(buff, 0, LINE_LENGTH);
			}
			pthread_mutex_lock(&m2);
			List_prepend(outputList, buff);
			pthread_mutex_unlock(&m2);
		}
		free(buff);
		if(strstr(buff, "!exit") != NULL){
			EXIT_FLAG = true;
		}
		
	}
}

void sender(struct SenderArguments *args){
	char *statusRequest = malloc(10);
	strcpy(statusRequest, "!status");
	encrypt(statusRequest);
	sendto(args->socket_fd, statusRequest, sizeof(statusRequest), MSG_DONTWAIT, (const struct sockaddr *) &args->addr, sizeof(args->addr));
	free(statusRequest);
	while(REMOTE_STATUS != true){}
	while(EXIT_FLAG != true){
		if(List_count(inputList) > 0){
			sendMsg(inputList, args->socket_fd, args->addr);
		}
	}
}

void sendMsg(List* inputList, int socket_fd, struct sockaddr_in dest){
	char *temp = malloc(LINE_LENGTH);
	memset(temp, 0, LINE_LENGTH);
	pthread_mutex_lock(&m1);
	strcpy(temp, (char *)List_trim(inputList));
	pthread_mutex_unlock(&m1);
	if(strstr(temp, "!exit") != NULL){
		EXIT_FLAG = true;
	}else if(strstr(temp, "!status") != NULL){
		free(temp);
		return;
	} 
	encrypt(temp);
	sendto(socket_fd, (const char *)temp, strlen(temp), MSG_CONFIRM, 
		(const struct sockaddr *) &dest, sizeof(dest));
	free(temp);
}

void encrypt(char* input){
	for(int i = 0; (input[i] != '\0'); i++){
		input[i] = input[i] + ENCRYPTION_KEY;
	}
}

void decrypt(char* input){
	for(int i = 0; (input[i] != '\0'); i++){
		input[i] = input[i] - ENCRYPTION_KEY;
	}
}