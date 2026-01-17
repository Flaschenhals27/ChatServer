#include <stddef.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "network.h"
#include <string.h>
#include "util.h"
#define SERVER_NAME "ChatServer-GROUP27"

//--- Sicherstellen das alle Bytes gesendet werden ---//
static int sendAll(int fd, const void *data, size_t len) {
    size_t sent = 0;
    const char *p = (const char *) data;
    while (sent < len) {
        ssize_t res = send(fd, p + sent, len - sent, MSG_NOSIGNAL); // Ausführen bis wirklich alles gesendet ist, startet immer bei Stelle der Daten + was bereits gesendet wurde
        if (res == -1) return -1; // Nur bei echten Fehlern -> Ganzer Abbruch
        sent += res;
    }
    return 0;
}

//--- Sicherstellen das alle Bytes empfangen wurden ---//
int networkReceive(int fd, void *buffer, size_t size) {
    size_t read_total = 0;
    char *p = buffer;
    while (read_total < size) {
        ssize_t res = recv(fd, p + read_total, size - read_total, 0);
        if (res <= 0) return (int) res; //Verbindungsabbruch oder Fehler
        read_total += res;
    }
    return 1;
}

//--- Funktionen für verschiedene Message Typen ---//
int sendLoginResponse(int fd, uint8_t code) {
    //- Ein Body wird zusammengebaut aufgrund des RFC-Protokolls -//
    //- RFC: Header + Magic(4) + Code(1) + ServerName(var) -//
    Header hdr;
    LoginResponseBody body;

    hdr.type = MT_LOGIN_RESPONSE;
    body.magic = htonl(MAGIC_RESPONSE);
    body.code = code;
    memset(body.server_name, 0, sizeof(body.server_name));
    strncpy(body.server_name, SERVER_NAME, 31);


    //- Die Laenge des Pakets berechnen, Namen soll nur so lang sein wie er wirklich ist -//
    uint16_t name_len = strnlen(body.server_name, 31);
    uint16_t total_len = 4 + 1 + name_len;
    hdr.length = htons(total_len);

    //- Header, Body und Servernamen nacheinander senden -//
    if (sendAll(fd, &hdr, sizeof(hdr)) == -1) return -1;
    if (sendAll(fd, &body, 5) == -1) return -1; //Body = Magic + Code
    if (sendAll(fd, body.server_name, name_len) == -1) return -1;

    return 0;
}


int sendServer2Client(int fd, const char *sender, const char *text, uint64_t timestamp) {
    //- RFC: Header + Timestamp(8) + OriginalSender(32) + Text(var) -//
    Server2ClientBody body = {0};
    Header hdr;

    hdr.type = MT_SERVER_TO_CLIENT;
    body.timestamp = hton64u(timestamp);
    if (sender) strncpy(body.original_sender, sender, 31); //Falls keine Systemnachricht wie Admin-Befehle, Fehlermeldungen oder Status-Inos wie "User Kicked"
    size_t text_len = strnlen(text, 512);
    strncpy(body.text, text, text_len);

    //- Gesamtlaenge berechnen des Pakets -//
    uint16_t total_len = 8 + 32 + text_len;
    hdr.length = htons(total_len);

    //- Header, Timestamp, Sender und Text nacheinander senden -//
    if (sendAll(fd, &hdr, sizeof(hdr)) == -1) return -1;
    if (sendAll(fd, &body.timestamp, 8) == -1) return -1;
    if (sendAll(fd, body.original_sender, 32) == -1) return -1;
    if (sendAll(fd, body.text, text_len) == -1) return -1;

    return 0;
}


int sendUserAdded(int fd, const char *name, uint64_t timestamp) {
    //- RFC: Header + Timestamp(8) + Name(var) -//
    UserAddedBody body;
    Header hdr;

    hdr.type = MT_USER_ADDED;
    body.timestamp = hton64u(timestamp);
    memset(body.name, 0, 32);
    strncpy(body.name, name, 31);

    //- Gesamtlaenge berechnen des Pakets -//
    uint16_t name_len = strnlen(body.name, 31);
    hdr.length = htons(8 + name_len);

    //- Header, Timestamp und Name senden -//
    if (sendAll(fd, &hdr, sizeof(hdr)) == -1) return -1;
    if (sendAll(fd, &body.timestamp, 8) == -1) return -1;
    if (sendAll(fd, body.name, name_len) == -1) return -1;

    return 0;
}

int sendUserRemoved(int fd, const char *name, uint8_t code, uint64_t timestamp) {
    //- RFC: Header + Timestamp(8) + Code(1) + Name(var) -//
    UserRemovedBody body;
    Header hdr;

    hdr.type = MT_USER_REMOVED;
    body.timestamp = hton64u(timestamp);
    body.code = code;
    memset(body.name, 0, 32);
    strncpy(body.name, name, 31);

    //- Gesamtlaenge berechnen des Pakets -//
    uint16_t name_len = strnlen(body.name, 31);
    hdr.length = htons(8 + 1 + name_len);

    //- Header, Timestamp, Code und Name senden -//
    if (sendAll(fd, &hdr, sizeof(hdr)) == -1) return -1;
    if (sendAll(fd, &body.timestamp, 8) == -1) return -1;
    if (sendAll(fd, &body.code, 1) == -1) return -1;
    if (sendAll(fd, body.name, name_len) == -1) return -1;

    return 0;
}
