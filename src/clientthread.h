#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H
#include "user.h"

void *clientthread(void *arg);
void sendMessage(User *target, void *msg);
void sendChatMessage(User *target, void *arg);

typedef struct {
    User *sender;
    char *text;
} ChatContext;

#endif
