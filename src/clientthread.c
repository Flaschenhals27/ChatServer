#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <endian.h>

#include "clientthread.h"
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
		infoPrint("Client hat keine Login Anfrage gesendet.\nTrenne Verbindung...");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.2 Auf Magic Number prüfen

	uint32_t magic = ntohl(*(uint32_t*)msg.text);

	if (magic != MAGIC_LOGIN_REQ) {
		infoPrint("Magic Number Error");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.3 Version überprüfen

	uint8_t version = msg.text[4];

	if (version != 0) {
		infoPrint("Falsche Protokoll Version.\n Trenne Verbindung...");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	//1.4 Namen extrahieren und prüfen

	int name_len = msg.len -5;

	if (name_len < 1 || name_len > MSG_MAX) {
		infoPrint("Login fehlgeschlagen: Name zu kurz oder zu lang.\n");
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	size_t valid_chars = nameBytesValidate(msg.text+5, name_len);

	if (valid_chars != name_len) {
		infoPrint("Login fehlgeschlagen: Ungültige Zeichen im Namen.");

		// WICHTIG: Dem Client sagen, warum er fliegt (Code 2)
		Message errorResp;
		errorResp.optcode = MSG_LOGIN_RESP;

		uint32_t magic = htonl(MAGIC_LOGIN_RESP);
		memcpy(errorResp.text, &magic, 4);

		// CODE 2 = Name Invalid (Laut Protokoll)
		errorResp.text[4] = 2;

		// Server Name muss trotzdem mit
		char *srvName = "ChatServer";
		strcpy(errorResp.text + 5, srvName);
		errorResp.len = 5 + strlen(srvName);

		networkSend(self->sock, &errorResp);

		// Rauswerfen
		close(self->sock);
		removeUser(self);
		return NULL;
	}

	memcpy(self->name, msg.text+5, name_len);
	self->name[name_len] = '\0';

	infoPrint("Login erfolgreich: User '%s' (Socket %d)\n", self->name, self->sock);

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
		infoPrint("Fehler beim Senden an %d\n", self->sock);
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
			msg.text[msg.len] = '\0';

			if (msg.text[0] == '/') {
				if (strcmp(self->name, "Admin") != 0) {
					errorPrint("User %s hat keine Admin Rechte!", self->name);
					continue;
				}
				if (strncmp(msg.text, "/pause", 6) == 0) {
					adminPause();
				}
				else if (strncmp(msg.text, "/resume", 7) == 0) {
					adminResume();
				}
				else if (strncmp(msg.text, "/kick ", 6) == 0) {
					char *targetName = msg.text+6;
					targetName[strcspn(targetName, "\r\n")] = '\0';
					User *victim = findUserByName(targetName);
					if (victim != NULL) {
						infoPrint("Kicking user '%s'...", victim->name);
						Message kickMsg;

						//User informieren das er gekickt wird
						Message byeMsg;
						byeMsg.optcode = MSG_SERVER_TO_CLIENT;

						uint64_t ts = hton64u((uint64_t)time(NULL));
						memcpy(byeMsg.text, &ts, 8);
						memset(byeMsg.text+8, 0, 32);
						strcpy(byeMsg.text+8, "Server");

						char *reason = "Du wurdest gekickt!";
						strcpy(byeMsg.text+40, reason);
						byeMsg.len = 40+strlen(reason);
						networkSend(victim->sock, &byeMsg);

						//Den anderen sagen das er gekickt wurde
						buildUserRemovedMsg(&kickMsg, victim->name, 1);
						broadcastQueueSend(&kickMsg);

						shutdown(victim->sock, SHUT_RDWR); //Socket schliesst sich beim nächsten recv
						close(victim->sock);
					}
				} else infoPrint("User gab unbekannten Befehl ein: %s", msg.text);
				continue;
			}
			infoPrint("%s: %s\n", self->name, msg.text);
			//Paket bauen
			Message chatMsg;
			chatMsg.optcode = MSG_SERVER_TO_CLIENT;

			uint64_t timestamp = hton64u((uint64_t)time(NULL));
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
	buildUserRemovedMsg(&byeMsg, self->name, 0);
	broadcastQueueSend(&byeMsg);

	close(self->sock);
	removeUser(self);
	return NULL;
}

void buildUserAddedMsg(Message *msg, char *username, uint64_t timestamp) {
	msg->optcode = MSG_USER_ADDED;
	uint64_t ts = ntoh64u(timestamp);
	memcpy(msg->text, &ts, 8);
	strncpy(msg->text+8, username, MSG_MAX-9);
	msg->text[MSG_MAX-1] = '\0';
	msg->len = 8+strlen(msg->text+8);
}

void buildUserRemovedMsg(Message *msg, char *username, uint8_t reason) { //0=Logout, 1=Kicked, 2=Error
	msg->optcode = MSG_USER_REMOVED;
	uint64_t ts = ntoh64u((uint64_t)time(NULL)); //Time könnte auch in andere Variable geschrieben werden, deswegen hier NULL
	memcpy(msg->text, &ts, 8);
	msg->text[8] = reason;
	strcpy(msg->text+9, username);
	msg->len = 9 + strlen(username);
}











