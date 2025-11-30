#include <errno.h>
#include "network.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>


int networkReceive(int fd, Message *buffer)
{
	uint8_t optcode;
	uint16_t net_len;
	int bytes;

	bytes = recv(fd, &optcode, sizeof(uint8_t), MSG_WAITALL);
	if (bytes <= 0) return -1;

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
	uint8_t optcode;
	int sent;
	size_t text_len = strlen(buffer->text);

	if (text_len > MSG_MAX) { //Nachricht zu lang?
		errno = EMSGSIZE;
		return -1;
	}

	uint16_t net_len = htons((uint16_t)text_len);

	optcode = 1; //Vorübergehend

	if (send(fd, &optcode, sizeof(uint8_t), 0) != 1) {
		return -1;
	}

	if (send(fd, &net_len, sizeof(uint16_t), 0) != sizeof(uint16_t)) {
		return -1;
	}

	if (text_len > 0) {
		sent = send(fd, buffer->text, text_len, 0);
		if (sent != text_len) return -1;
	}

	return 0;
}