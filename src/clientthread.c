#include "clientthread.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <endian.h>
#include <stdlib.h>

#include "network.h"
#include "user.h"
#include "util.h"
#include "broadcastagent.h"

void sendListEntryToMe(User *existingUser, void *arg) {
	User *self = (User *)arg;

	if (existingUser != self) {
		Message listMsg;
		buildUserAddedMsg(&listMsg, existingUser->name, 0);
		networkSend(self->sock, &listMsg);
	}
}

void *clientthread(void *arg) {
	User *self = (User *)arg;
	Message msg;

	debugPrint("Thread gestartet. Warte auf Login...");

	//1. Login Request verarbeiten

	if (networkReceive(self->sock, &msg) == -1) {
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.1 Prüfen ob Login Request

	if (msg.optcode != MSG_LOGIN_REQ) {
		printf("Client hat keine Login Anfrage gesendet.\nTrenne Verbindung...");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.2 Auf Magic Number prüfen

	uint32_t magic = ntohl(*(uint32_t*)msg.text);

	if (magic != MAGIC_LOGIN_REQ) {
		printf("Magic Number Error");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.3 Version überprüfen

	uint8_t version = msg.text[4];

	if (version != 0) {
		printf("Falsche Protokoll Version.\n Trenne Verbindung...");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.4 Namen extrahieren und prüfen

	int name_len = msg.len -5;

	if (name_len < 1 || name_len > MSG_MAX) {
		printf("Login fehlgeschlagen: Name zu kurz oder zu lang.\n");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	memcpy(self->name, msg.text+5, name_len);
	self->name[name_len] = '\0';

	printf("Login erfolgreich: User '%s' (Socket %d)\n", self->name, self->sock);

	//2. Login Response senden

	Message response;
	response.optcode = MSG_LOGIN_RESP;

	uint32_t magic_resp = htonl(MAGIC_LOGIN_RESP);
	memcpy(response.text, &magic_resp, 4);

	response.text[4] = 0;

	char *serverName = "ChatServer_v1";
	int srv_len = strlen(serverName);
	memcpy(response.text+5, serverName, srv_len);

	response.len = 5+srv_len;

	if (networkSend(self->sock, &response) == -1) {
		printf("Fehler beim Senden an %d\n", self->sock);
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	char joinMsg[MSG_MAX];
	snprintf(joinMsg, sizeof(joinMsg), ">>> %s ist beigetreten.", self->name);

	//Teil für die Liste des GUI-Clients
	Message welcomeMsg;
	buildUserAddedMsg(&welcomeMsg, self->name, (uint64_t)time(NULL));
	broadcastQueueSend(&welcomeMsg);

	iterateUser(NULL, sendListEntryToMe, self);

	while (1) {
		if (networkReceive(self->sock, &msg) == -1) break;

		if (msg.optcode == MSG_CLIENT_TO_SERVER) {
			printf("%s: %s\n", self->name, msg.text);
			//Paket bauen
			Message chatMsg;
			chatMsg.optcode = MSG_SERVER_TO_CLIENT;

			uint64_t timestamp = htobe64((uint64_t)time(NULL));
			memcpy(chatMsg.text, &timestamp, 8);

			memset(chatMsg.text+8, 0, 32); //Füllt den Speicherbereich mit 0 auf (Sicherheit)
			strncpy(chatMsg.text+8, self->name, 31);

			int offset = 40;
			strncpy(chatMsg.text+offset, msg.text, MSG_MAX-offset-1);
			chatMsg.text[MSG_MAX-1] = '\0';
			chatMsg.len = offset+strlen(chatMsg.text+offset);

			broadcastQueueSend(&chatMsg);
		}
	}

	Message byeMsg;
	buildUserRemovedMsg(&byeMsg, self->name);
	broadcastQueueSend(&byeMsg);

	close(self->sock);
	removeUser(self);
	return NULL;
}

void buildUserAddedMsg(Message *msg, char *username, uint64_t timestamp) {
	msg->optcode = MSG_USER_ADDED;
	uint64_t ts = htobe64(timestamp);
	memcpy(msg->text, &ts, 8);
	strncpy(msg->text+8, username, MSG_MAX-9);
	msg->text[MSG_MAX-1] = '\0';
	msg->len = 8+strlen(msg->text+8);
}

void buildUserRemovedMsg(Message *msg, char *username) {
	msg->optcode = MSG_USER_REMOVED;
	uint64_t ts = htobe64((uint64_t)time(NULL)); //Time könnte auch in andere Variable geschrieben werden, deswegen hier NULL
	memcpy(msg->text, &ts, 8);
	msg->text[8] = CONNECTION_CLOSED_BY_CLIENT;
	strncpy(msg->text+9, username, MSG_MAX-10);
	msg->text[MSG_MAX-1] = '\0';
	msg->len = 9 + strlen(msg->text+9);
}











