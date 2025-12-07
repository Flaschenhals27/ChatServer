#include <pthread.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#include "broadcastagent.h"

#include <errno.h>
#include <asm-generic/errno.h>

#include "network.h"
#include "user.h"
#include "util.h"

#define QUEUE_NAME "/chat_server_broadcast"
#define QUEUE_MAX_MSGS 10

static sem_t pauseSemaphore;
static int isPaused = 0;
static mqd_t messageQueue;
static pthread_t threadId;

static void *broadcastAgent(void *arg)
{
	//Wie groß ist die Queue wirklich? ->Attribute holen
	struct mq_attr attr;
	if (mq_getattr(messageQueue, &attr) < 0) {
		errnoPrint("Broadcast Agent: Konnte Queue-Attribute nicht lesen");
		return NULL;
	}

	//Dynamischen Puffer erstellen
	char *buffer = malloc(attr.mq_msgsize);
	if (buffer == NULL) {
		errnoPrint("Broadcast Agent: Speicherreservirung für Queue fehlgeschlagen");
		return NULL;
	}

	debugPrint("Broadcast Agent: Thread gestartet. Warte auf Post...");

	while (1) {
		ssize_t bytes_read = mq_receive(messageQueue, buffer, attr.mq_msgsize, NULL);

		if (bytes_read < 0) {
			continue; //Fehler ignorieren...
		}
		sem_wait(&pauseSemaphore);
		sem_post(&pauseSemaphore);

		Message *msg = (Message *)buffer; //Puffer als Message casten
		iterateUser(NULL, sendToSocket, msg); //Nachricht verteilen
	}
	free(buffer);
	return NULL;
}

void sendToSocket(User *target, void *arg) {
	Message *msg = (Message *)arg;
	networkSend(target->sock, msg);
}

int broadcastAgentInit(void)
{
	mq_unlink(QUEUE_NAME);
	struct mq_attr attr;
	attr.mq_flags = 0; //Keine Flags
	attr.mq_maxmsg = QUEUE_MAX_MSGS; //Maximale Anzahl an Nachrichten
	attr.mq_msgsize = sizeof(Message); //Größe einer Nachricht
	attr.mq_curmsgs = 0;

	messageQueue = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, NULL);

	if (messageQueue == (mqd_t)-1) {
		errnoPrint("Broadcast Agent: mq_open failed");
		return -1;
	}

	if (sem_init(&pauseSemaphore, 0, 1) == -1) {
		errnoPrint("Broadcast Agent: sem_init failed");
		return -1;
	}

	if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
		errnoPrint("Broadcast Agent: pthread_create failed");
		mq_close(messageQueue);
		mq_unlink(QUEUE_NAME);
		return -1;
	}
	return 0;
}

void broadcastAgentCleanup(void)
{
	pthread_cancel(threadId); //Thread abbrechen
	pthread_join(threadId, NULL);
	mq_close(messageQueue); //Queue schliesen
	mq_unlink(QUEUE_NAME); //Queue löschen
}

int broadcastQueueSend(const Message *msg) {

	if (isPaused) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 1;

		if (mq_timedsend(messageQueue, (const char *)msg, sizeof(Message), 0, &ts) == -1) { //Wartet eine Sekunde darauf die Nachricht einwerfen zu können. Nutzt die Systemzeit (sicherer). Wenn Zeit abgelaufen, Nachricht verwerfen.
			if (errno = ETIMEDOUT) {
				infoPrint("Nachricht verworfen. (Server pausiert und Queue voll)\n");
				return -2; //Extra Code für 'Voll'
			}
			errorPrint("mq_timedsend failed");
			return -1;
		}
	} else {
		if (mq_send(messageQueue, (const char *)msg, sizeof(Message), 0) == -1) {
			errnoPrint("Broadcast Agent: mq_send failed");
			return -1;
		}
	}
	return 0;
}

void adminPause(void) {
	if (!isPaused) {
		sem_wait(&pauseSemaphore);
		isPaused = 1;
		infoPrint("System: Broadcast Agent pausiert.\n");
	}
}

void adminResume(void) {
	if (isPaused) {
		sem_post(&pauseSemaphore);
		isPaused = 0;
		infoPrint("System: Broadcast Agent läuft wieder.\n");
	}
}