#include <pthread.h>
#include <mqueue.h>
#include "broadcastagent.h"

#include "network.h"
#include "user.h"
#include "util.h"

static mqd_t messageQueue;
static pthread_t threadId;

static void *broadcastAgent(void *arg)
{
	Message msg;
	ssize_t bytes_read; //Ist vorzeichenbehaftet, benötigt für Fehlercodes (-1)

	debugPrint("Broadcast Agent: Thread gestartet. Warte auf Post...");

	while (1) {

	}
}

void sendToSocket(User *target, void *arg) {
	Message *msg = (Message *)arg;
	networkSend(target->sock, msg);
}

int broadcastAgentInit(void)
{
	//TODO: create message queue
	//TODO: start thread
	return -1;
}

void broadcastAgentCleanup(void)
{
	//TODO: stop thread
	//TODO: destroy message queue
}
