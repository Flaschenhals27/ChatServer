#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H
#define CONNECTION_CLOSED_BY_CLIENT 0
#define CONNECTION_CLOSED_BY_SERVER 1
#define CONNECTION_ERROR 2
#include <stdint.h>

#include "network.h"
#include "user.h"

void *clientthread(void *arg);
void sendMessage(User *target, void *msg);
void sendChatMessage(User *target, void *arg);
void sendListEntryToMe(User *existingUser, void *arg);
void buildUserAddedMsg(Message *msg, char *username, uint64_t timestamp);
void buildUserRemovedMsg(Message *msg, char *username);

#endif
