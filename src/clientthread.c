#include "clientthread.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <endian.h>

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

	msg.optcode = MSG_SERVER_TO_CLIENT;

	strncpy(msg.text, text, MSG_MAX-1);
	msg.text[MSG_MAX-1] = '\0';
	msg.len = strlen(msg.text);
	if (networkSend(target->sock, &msg) == -1) {
		printf("Fehler beim Senden an %d\n", target->sock);
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

	while (1) {
		if (networkReceive(self->sock, &msg) == -1) break;

		if (msg.optcode == MSG_CLIENT_TO_SERVER) {
			printf("%s: %s\n", self->name, msg.text);

			ChatContext ctx;
			ctx.sender = self;
			ctx.text = msg.text;
			iterateUser(NULL, sendChatMessage, &ctx);
		}
	}
	close(self->sock);
	removeUser(self);
	return NULL;
}

void sendChatMessage(User *target, void *arg) {
	ChatContext * ctx = (ChatContext *)arg;

	Message msg;
	msg.optcode = MSG_SERVER_TO_CLIENT;

	uint64_t timestamp = htobe64((uint64_t)time(NULL));
	memcpy(msg.text, &timestamp, 8);

	memset(msg.text+8, 0, 32);
	strncpy(msg.text+8, ctx->sender->name, 31);

	int offset = 40; //8Byte Zeit+32 Name
	strncpy(msg.text + offset, ctx->text, MSG_MAX-offset-1);
	msg.text[MSG_MAX-1] = '\0';

	msg.len = offset + strlen(msg.text+offset);
	networkSend(target->sock, &msg);
}
