#include "clientthread.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "network.h"
#include "user.h"
#include "util.h"

/*void *clientthread(void *arg)
{
	User *self = (User *)arg;
	Message msg;
	int bytes_read;

	debugPrint("Client thread started.");

	//Erste Nachricht ist der Name
	if (networkReceive(self->sock, &msg) == -1) { //Wenn kein Name genannt, Verbindung schließen
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	strncpy(self->name, msg.text, MAX_NAME_LENGTH-1);
	self->name[MAX_NAME_LENGTH-1] = '\0';
	self->name[strcspn(self->name, "\n")] = '\0'; //Suche das Enter, beende da den Namen

	char joinMessage[MAX_NAME_LENGTH];
	snprintf(joinMessage, MAX_NAME_LENGTH-1, "%s ist dem Chat beigetreten.", self->name);
	iterateUser(self, sendMessage, joinMessage);

	while (1) {
		if (networkReceive(self->sock, &msg) == -1) {
			break;
		}
		printf("Empfangen von %d: %s\n", self->sock, msg.text);
		iterateUser(self, sendMessage, msg.text);
	}

	close(self->sock);
	removeUser(self);
	debugPrint("Client thread stopping.");
	return NULL;
}*/

void sendMessage(User *target, void *arg) {
	char *text = (char *)arg;
	printf("Sende an User %d: %s\n", target->sock, text);
	Message msg;

	strncpy(msg.text, text, MSG_MAX-1);
	msg.text[MSG_MAX-1] = '\0';
	if (networkSend(target->sock, &msg) == -1) {
		printf("Fehler beim Senden an %d\n", target->sock);
	}
}

void *clientthread(void *arg) {
	User *self = (User *)arg;
	Message msg;

	debugPrint("Thread gestartet. Warte auf Login...");

	//Login Request verarbeiten

	if (networkReceive(self->sock, &msg) == -1) {
		close(self->sock);
		removeUser(self);
		return NULL;
	}
}
