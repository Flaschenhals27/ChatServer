#ifndef BROADCASTAGENT_H
#define BROADCASTAGENT_H
#include "network.h"
#include "user.h"

int broadcastAgentInit(void);
void broadcastAgentCleanup(void);
int broadcastQueueSend(const Message *msg);
void sendToSocket(User *target, void *arg);

#endif
