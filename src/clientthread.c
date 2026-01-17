#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>       // Für Zeitstempel (time())
#include <arpa/inet.h>  // Für ntohl/ntohs
#include <sys/socket.h>

#include "clientthread.h"
#include "user.h"
#include "util.h"
#include "network.h"
#include "broadcastagent.h"
//- Verwaltet die Threads; Jeder Thread hat eigenen File Descriptor -//
static __thread int g_new_client_fd;

//--- Adapter der sendUserAdded aufruft. Wird in userIterate aufgerufen um alle Benutzer zu informieren ---//
static void send_existing_user(User *existing_user) {
    // Timestamp 0 signalisiert, User war schon vorher da
    if (existing_user->sock == g_new_client_fd) {
        return;
    }

    if (strlen(existing_user->name) == 0) return; //- Netcatlauscher ausschliessen -//
    sendUserAdded(g_new_client_fd, existing_user->name, 0);
}

void *clientthread(void *arg) {
    User *self = arg; //- Impliziter Cast, explizit nicht noetig in C -//
    Header hdr;

    debugPrint("New connection handling started on socket %d", self->sock);

    //--- Handshake starten ---//
    //- Header lesen und pruefen ob Login Anfrage -//
    if (networkReceive(self->sock, &hdr, sizeof(Header)) < 0) goto cleanup;

    if (hdr.type != MT_LOGIN_REQUEST) {
        errorPrint("Client sent msg type %d instead of LoginRequest!", hdr.type);
        goto cleanup;
    }

    //- Body lesen, maximal so gross wie in LoginRequestBody passt -//
    LoginRequestBody loginReq;
    uint16_t bodyLen = ntohs(hdr.length);

    if (bodyLen > sizeof(LoginRequestBody)) bodyLen = sizeof(LoginRequestBody);

    if (networkReceive(self->sock, &loginReq, bodyLen) <= 0) goto cleanup;

    //- Validierung (Magic Number, Version, Name frei?) -//
    uint8_t respCode = LC_SUCCESS;

    if (ntohl(loginReq.magic) != MAGIC_REQUEST) {
        respCode = LC_ERROR; //- Protokoll nicht eingehalten / Falsche Magic Number -//
    } else if (loginReq.version != PROT_VERSION) {
        respCode = LC_VERSION_MISMATCH; //- Client veraltet -//
    } else {
        char tempName[32] = {0};

        //- Name beginnt nach Magic(4) und Version (1) -//
        if (bodyLen > 5) {
            size_t nameLen = bodyLen - 5;
            if (nameLen > 31) nameLen = 31;
            memcpy(tempName, loginReq.name, nameLen); //- Maximal ist der Name 32Byte land -//
        }

        //- Gueltigkeit des Namens ueberpruefen -//
        int name_is_valid = 1;
        int namelen = (int) strlen(tempName);
        for (int i = 0; i < namelen; i++) {
            unsigned char c = (unsigned char) tempName[i];
            if (c == 34 || c == 39 || c == 96 || c < 32 || c > 126) { //- ASCII Codierung -//
                name_is_valid = 0;
                break;
            }
        }

        if (!name_is_valid) {
            respCode = LC_NAME_INVALID;
        }

        //- Pruefen ob Name schon vergeben -//
        else if (user_find(tempName) != NULL) {
            respCode = LC_NAME_TAKEN;
        } else {
            strncpy(self->name, tempName, 31);
        }
    }

    //- Login Response senden; Falls ein Fehler vorliegt, Cleanup starten -//
    if (sendLoginResponse(self->sock, respCode) == -1) goto cleanup;
    if (respCode != LC_SUCCESS) {
        infoPrint("Login failed (Code: %d)", respCode);
        goto cleanup;
    }

    infoPrint("User logged in: %s", self->name);

    //- User ist eingeloggt; Dem neuen User die alten anzeigen -//
    g_new_client_fd = self->sock;
    user_iterate(send_existing_user);

    //- Alle alten User den neuen uebergeben -//
    InternalMessage uadMsg;
    uadMsg.type = MT_USER_ADDED;
    uadMsg.data.uad.timestamp = (uint64_t) time(NULL);
    strncpy(uadMsg.data.uad.name, self->name, 31);

    broadcastQueueSend(&uadMsg);

    //- Chatloop -//
    while (1) {
        //- Header und Body (max 512Byte) lesen -//
        if (networkReceive(self->sock, &hdr, sizeof(Header)) <= 0) break;

        uint16_t len = ntohs(hdr.length);
        char textBuffer[513];
        if (len > 512) len = 512;

        if (networkReceive(self->sock, textBuffer, len) <= 0) break;
        textBuffer[len] = '\0';

        //- Verarbeiten -//
        if (hdr.type == MT_CLIENT_TO_SERVER) {
            uint64_t timestamp = (uint64_t) time(NULL);
            //- Auf Admin Nachricht pruefen -//
            if (textBuffer[0] == '/') {
                //- Falls nicht vom Admin, keine Befehle durchsetzen -//
                if (strcmp(self->name, "Admin") != 0) {
                    sendServer2Client(self->sock, NULL, "Permission denied!", timestamp);
                    continue;
                }

                //- Pause -//
                if (strcmp(textBuffer, "/pause") == 0) {
                    if (broadcastStop() == 0) {
                        InternalMessage msg;
                        msg.type = MT_SERVER_TO_CLIENT;
                        msg.data.s2c.timestamp = timestamp;
                        memset(msg.data.s2c.original_sender, 0, 32); // Absender leer = Servernachricht
                        strncpy(msg.data.s2c.text, "Server paused.", 512);
                        broadcastQueueSend(&msg);
                    } else {
                        sendServer2Client(self->sock, NULL, "Error: Server already paused.", timestamp);
                    }
                }
                //- Resume -//
                else if (strcmp(textBuffer, "/resume") == 0) {
                    if (broadcastResume() == 0) {
                        InternalMessage msg;
                        msg.type = MT_SERVER_TO_CLIENT;
                        msg.data.s2c.timestamp = timestamp;
                        memset(msg.data.s2c.original_sender, 0, 32); // Absender leer
                        strncpy(msg.data.s2c.text, "Server resumed.", 512);
                        broadcastQueueSend(&msg);
                    } else {
                        sendServer2Client(self->sock, NULL, "Error: Server not paused.", timestamp);
                    }
                }
                //- Kick -//
                else if (strncmp(textBuffer, "/kick ", 6) == 0) {
                    char *victimName = textBuffer + 6;
                    User *victim = user_find(victimName);

                    if (victim) {
                        victim->closeReason=1;
                        shutdown(victim->sock, SHUT_RDWR);
                        close(victim->sock);
                    } else {
                        sendServer2Client(self->sock, NULL, "User not found.", timestamp);
                    }
                } else {
                    sendServer2Client(self->sock, NULL, "Unknown command.", timestamp);
                }
            }
            //- Normale Nachricht -//
            else {
                InternalMessage bcast;
                bcast.type = MT_SERVER_TO_CLIENT;
                bcast.data.s2c.timestamp = (uint64_t) time(NULL);
                strncpy(bcast.data.s2c.original_sender, self->name, 31);
                strncpy(bcast.data.s2c.text, textBuffer, 512);

                if (broadcastQueueSend(&bcast) == -1) {
                    sendServer2Client(self->sock, NULL, "Error: Server is busy (Queue full). Message dropped.", bcast.data.s2c.timestamp);
                }
            }
        }
    }

    //- Cleanup Goto -//
cleanup:
    debugPrint("Client thread stopping for %s.", self->name);

    char savedName[32];
    strncpy(savedName, self->name, 32);
    int savedIsKicked = self->closeReason;
    int hasName = (strlen(savedName) > 0);

    user_remove(self);

    if (hasName) {
        InternalMessage urmMsg;
        urmMsg.type = MT_USER_REMOVED;
        urmMsg.data.urm.timestamp = (uint64_t) time(NULL);

        switch (savedIsKicked) {
            case 0: urmMsg.data.urm.code = 0; break; //- 0 = Connection closed by client -//
            case 1: urmMsg.data.urm.code = 1; break; //- 1 = Kicked by Admin -//
            case 2: urmMsg.data.urm.code = 2; break; //- 2 = Communication Error -//
            default: urmMsg.data.urm.code = 0; break;
        }

        strncpy(urmMsg.data.urm.name, savedName, 32);

        broadcastQueueSend(&urmMsg);
    }

    return NULL;
}
