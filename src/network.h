#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdint.h>

// Constants & Magic Numbers out of RFC
// Dienen der Indentifikation eines Nachrichten Pakets
#define MAGIC_REQUEST  0x0badf00d //Anfang Login Requests
#define MAGIC_RESPONSE 0xc001c001 //Anfang Login Response
#define PROT_VERSION   0 // Protokoll Version 0

// Message Types
enum MessageType {
    MT_LOGIN_REQUEST = 0,
    MT_LOGIN_RESPONSE = 1,
    MT_CLIENT_TO_SERVER = 2,
    MT_SERVER_TO_CLIENT = 3,
    MT_USER_ADDED = 4,
    MT_USER_REMOVED = 5
};

// Failure Codes for Login Response
enum LoginCode {
    LC_SUCCESS = 0,
    LC_NAME_TAKEN = 1,
    LC_NAME_INVALID = 2,
    LC_VERSION_MISMATCH = 3,
    LC_ERROR = 255
};

//Datastructures packed
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint16_t length;
} Header;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    char name[32];
} LoginRequestBody;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t code;
    char server_name[32];
} LoginResponseBody;

typedef struct __attribute__((packed)) {
    uint64_t timestamp;
    char original_sender[32];
    char text[512];
} Server2ClientBody;

typedef struct __attribute__((packed)) {
    uint64_t timestamp;
    char name[32];
} UserAddedBody;

typedef struct __attribute__((packed)) {
    uint64_t timestamp;
    uint8_t code;
    char name[32];
} UserRemovedBody;

// Container for internal Broadcast Queue
typedef struct {
    uint8_t type;

    union {
        Server2ClientBody s2c;
        UserAddedBody uad;
        UserRemovedBody urm;
    } data;
} InternalMessage;

int networkReceive(int fd, void *buffer, size_t size);

int sendLoginResponse(int fd, uint8_t code);

int sendServer2Client(int fd, const char *sender, const char *text, uint64_t timestamp);

int sendUserAdded(int fd, const char *name, uint64_t timestamp);

int sendUserRemoved(int fd, const char *name, uint8_t code, uint64_t timestamp);

#endif