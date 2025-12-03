#include <pthread.h>
#include <mqueue.h>
#include "broadcastagent.h"

#include <stdio.h>
#include <stdlib.h>

#include "network.h"
#include "user.h"
#include "util.h"
#define QUEUE_NAME "/chat_server_broadcast2342"
#define QUEUE_MAX_MSGS 10

static mqd_t messageQueue;
static pthread_t threadId;

static void *broadcastAgent(void *arg)
{
	//Wie groß ist die Queue wirklich? ->Attribute holen
	struct mq_attr attr;
	if (mq_getattr(messageQueue, &attr) < 0) {
		perror("Broadcast Agent: Konnte Queue-Attribute nicht lesen");
		return NULL;
	}

	//Dynamischen Puffer erstellen
	char *buffer = malloc(attr.mq_msgsize);
	if (buffer == NULL) {
		perror("Broadcast Agent: Speicherreservirung für Queue fehlgeschlagen");
		return NULL;
	}

	debugPrint("Broadcast Agent: Thread gestartet. Warte auf Post...");

	while (1) {
		ssize_t bytes_read = mq_receive(messageQueue, buffer, attr.mq_msgsize, NULL);
		if (bytes_read < 0) {
			continue; //Fehler ignorieren...
		}
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
		perror("Broadcast Agent: mq_open failed");
		return -1;
	}

	if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
		perror("Broadcast Agent: pthread_create failed");
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
	if (mq_send(messageQueue, (const char *)msg, sizeof(Message), 0) == -1) {
		perror("Broadcast Agent: mq_send failed");
		return -1;
	}
	return 0;
}
