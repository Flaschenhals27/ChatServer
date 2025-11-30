#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#define MAGIC_LOGIN_REQ 0x0badf00d
#define MAGIC_LOGIN_RESP 0xc001c001

#include <stdint.h>

/* TODO: When implementing the fully-featured network protocol (including
 * login), replace this with message structures derived from the network
 * protocol (RFC) as found in the moodle course. */
enum { MSG_MAX = 1024 };

typedef enum {
	MSG_LOGIN_REQ = 0,
	MSG_LOGIN_RESP = 1,
	MSG_CLIENT_TO_SERVER = 2,
	MSG_SERVER_TO_CLIENT = 3,
	MSG_USER_ADDED = 4,
	MSG_USER_REMOVED = 5
} MessageType;

typedef struct __attribute__((packed))
{
	uint16_t len;		//real length of the text (big endian, len <= MSG_MAX)
	char text[MSG_MAX];	//text message
	uint8_t optcode;
} Message;

int networkReceive(int fd, Message *buffer);
int networkSend(int fd, const Message *buffer);

#endif
