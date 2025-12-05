#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "network.h"

int networkReceive(int fd, Message *buffer)
{
	uint8_t optcode;
	uint16_t net_len;
	int bytes;

	bytes = recv(fd, &optcode, sizeof(uint8_t), MSG_WAITALL);
	if (bytes <= 0) return -1;
	buffer->optcode = optcode;

	bytes = recv(fd, &net_len, sizeof(uint16_t), MSG_WAITALL);
	if (bytes <= 0) return -1;

	buffer->len = ntohs(net_len);
	if (buffer->len > MSG_MAX) { //Protokoll Missachtung
		errno = EMSGSIZE;
		return -1;
	}

	if (buffer->len > 0) {
		bytes = recv(fd, buffer->text, buffer->len, MSG_WAITALL);
		if (bytes <= 0) return -1;

		if (bytes != buffer->len) { //Nicht alles empfangen
			errno = EIO; //Error Input/Output
			return -1;
		}
	}

	buffer->text[buffer->len] = '\0';
	return 0;
}

int networkSend(int fd, const Message *buffer)
{
	// 1. Wir nehmen die Länge, die im Struct steht.
	// (Wichtig für Pakete mit Nullen drin, wie Timestamp)
	uint16_t payload_len = buffer->len;

	// 2. Sicherheitscheck: Passt das überhaupt in den Puffer?
	if (payload_len > MSG_MAX) {
		errno = EMSGSIZE;
		return -1;
	}

	// 3. Optcode senden (1 Byte)
	// Wir senden exakt 1 Byte.
	if (send(fd, &buffer->optcode, 1, 0) != 1) {
		return -1;
	}

	// 4. Länge senden (2 Bytes)
	// Erst umwandeln in Network Byte Order (Big Endian)
	uint16_t net_len = htons(payload_len);

	if (send(fd, &net_len, sizeof(uint16_t), 0) != sizeof(uint16_t)) {
		return -1;
	}

	// 5. Inhalt senden (Payload)
	// Nur senden, wenn die Länge > 0 ist.
	if (payload_len > 0) {
		// Hier wird der Speicherbereich blind kopiert (auch Nullen!)
		if (send(fd, buffer->text, payload_len, 0) != payload_len) {
			return -1;
		}
	}

	return 0; // Alles gut gegangen
}