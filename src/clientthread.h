#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H
#include "user.h"

void *clientthread(void *arg);
void sendMessage(User *target, void *msg);

#endif
